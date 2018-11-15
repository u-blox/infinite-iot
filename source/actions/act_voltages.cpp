/* mbed Microcontroller Library
 * Copyright (c) 2006-2018 u-blox Limited
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <mbed.h>
#include <eh_config.h>
#include <act_voltages.h>

/**************************************************************************
 * MANIFEST CONSTANTS
 *************************************************************************/

// Convert an ADC reading to milliVolts.  A calibration run has it as:
// voltage in mV = (reading - 60) / 14.20
// Note: every so often I see some very strange values (e.g. 0x7ffffffc).
// I can't figure out where these are coming from so I limit the value
// here to avoid getting a silly reading on the web interface.
#define READING_TO_MV(reading) (((reading) >= 60) && ((reading) <= 65535) ? \
                                ((reading) - 60) * 1000 / 14200 : 0)

// Set a pin to thoroughly disconnected mode
#define DISCONNECT_PIN(pin)  nrf_gpio_cfg(pin,                           \
                                          NRF_GPIO_PIN_DIR_INPUT,        \
                                          NRF_GPIO_PIN_INPUT_DISCONNECT, \
                                          NRF_GPIO_PIN_NOPULL,           \
                                          NRF_GPIO_PIN_S0S1,             \
                                          NRF_GPIO_PIN_NOSENSE);

// Set the voltage divider to its "in use" state (standard pull down to 0)
#define ENABLE_VOLTAGE_MEASUREMENT     nrf_gpio_cfg(PIN_ENABLE_VOLTAGE_DIVIDERS,   \
                                                    NRF_GPIO_PIN_DIR_OUTPUT,       \
                                                    NRF_GPIO_PIN_INPUT_DISCONNECT, \
                                                    NRF_GPIO_PIN_NOPULL,           \
                                                    NRF_GPIO_PIN_S0D1,             \
                                                    NRF_GPIO_PIN_NOSENSE);         \
                                       nrf_gpio_pin_clear(PIN_ENABLE_VOLTAGE_DIVIDERS);

// Set the voltage divider pin to its "not in use" state
#define DISABLE_VOLTAGE_MEASUREMENT    DISCONNECT_PIN(PIN_ENABLE_VOLTAGE_DIVIDERS);

/**************************************************************************
 * LOCAL VARIABLES
 *************************************************************************/

// Fake power is good.
static bool gVoltageFakeIsGood = false;

// Fake power is bad.
static bool gVoltageFakeIsBad = false;

/**************************************************************************
 * STATIC FUNCTIONS
 *************************************************************************/

// Go through the operations to read a pin.
int getVoltage(PinName pin)
{
    int reading;
    AnalogIn *pV = new AnalogIn(pin);

    ENABLE_VOLTAGE_MEASUREMENT;

    Thread::wait(10);
    reading = pV->read_u16();
    reading = READING_TO_MV(reading);

    DISABLE_VOLTAGE_MEASUREMENT;
    delete pV;
    DISCONNECT_PIN(pin);

    return reading;
}

/**************************************************************************
 * PUBLIC FUNCTIONS
 *************************************************************************/

// Get the value of VBAT_OK.
int getVBatOkMV()
{
    return getVoltage(PIN_ANALOGUE_VBAT_OK);
}

// Get the value of VIN.
int getVInMV()
{
    return getVoltage(PIN_ANALOGUE_VIN);
}

// Get the value of VPRIMARY.
int getVPrimaryMV()
{
    return getVoltage(PIN_ANALOGUE_VPRIMARY);
}

// Get an estimate of the energy available.
unsigned long long int getEnergyAvailableNWH()
{
    unsigned long long int energyNWH = 0;
    unsigned int vBatOkMV = getVBatOkMV();

    // The energy available is a combination of that
    // stored in the supercap and that stored in the
    // secondary cell, the secondary cell restoring
    // the "delivery" charge in the supercap.
    // For relatively light loads the whole of the
    // secondary cell is the capacity but for high
    // loads the supercap will droop before it can
    // be recharged.  The intention here is to,
    // very roughly, model that behaviour.

    // The energy stored in a capacitor in Joules,
    // AKA Watt-seconds, is 0.5CV^2, where the units
    // are Farads and Volts.  We also need to take into
    // account that we can only take out some of the
    // charge, taking us down to VBAT_OK_BAD_THRESHOLD_MV.
    // For example, if we have a 0.47 Farad supercap charged to
    // 3.8 V and we can let it go down to 3.0 V then the
    // available energy is 1.2784 Watt seconds.  Converting
    // to nWh this is 355,111 nWh.
    //
    // J = (SUPERCAP_MICROFARADS / 2 * vBatOkMV * vBatOkMV / 1000 / 1000 / 1000000) -
    //     (SUPERCAP_MICROFARADS / 2 * VBAT_OK_BAD_THRESHOLD_MV * VBAT_OK_BAD_THRESHOLD_MV / 1000 / 1000 / 1000000)
    // nWh = J * 1000000000 / 3600
    if (vBatOkMV > VBAT_OK_BAD_THRESHOLD_MV) {
        energyNWH = (SUPERCAP_MICROFARADS / 2 * vBatOkMV * vBatOkMV / 1000 / 3600) -
                    (SUPERCAP_MICROFARADS / 2 * VBAT_OK_BAD_THRESHOLD_MV *
                     VBAT_OK_BAD_THRESHOLD_MV / 1000 / 3600);
    }

    // Now we need to make some sort of assumption as to
    // the level of charge in the secondary cell.
    // What follows is a complete guess.
    // Let's say that if the supercap has been returned to
    // VBAT_OK_GOOD_THRESHOLD_MV then the secondary cell
    // must be full charged, otherwise it cannot be relied
    // upon and we have to wait for it to perk back up.
    // TODO: this will need tweaking.
    if (((vBatOkMV > VBAT_OK_GOOD_THRESHOLD_MV) || gVoltageFakeIsGood) && !gVoltageFakeIsBad) {
        energyNWH += SECONDARY_BATTERY_CAPACITY_NWH;
    }

    return energyNWH;
}

// Check if VBAT_SEC is good enough to run everything from
bool voltageIsGood()
{
    bool vBatOk = false;

    // Check against the upper threshold for VBAT_OK.
    if (getVBatOkMV() >= VBAT_OK_GOOD_THRESHOLD_MV) {
        vBatOk = true;
    }

    return (vBatOk || gVoltageFakeIsGood) && !gVoltageFakeIsBad;
}

// Check if VBAT_SEC is good enough to run something from
bool voltageIsBearable()
{
    bool vBatOk = false;

    // Check against the mid threshold for VBAT_OK.
    if (getVBatOkMV() >= VBAT_OK_BEARABLE_THRESHOLD_MV) {
        vBatOk = true;
    }

    return (vBatOk || gVoltageFakeIsGood) && !gVoltageFakeIsBad;
}

// Check if VBAT_SEC is STILL good enough to run something from
bool voltageIsNotBad()
{
    bool vBatNotBad = false;

    // Check against the lower threshold for VBAT_OK.
    if (getVBatOkMV() >= VBAT_OK_BAD_THRESHOLD_MV) {
        vBatNotBad = true;
    }

    return (vBatNotBad || gVoltageFakeIsGood) && !gVoltageFakeIsBad;
}

// Fake power being good.
void voltageFakeIsGood(bool fake)
{
    gVoltageFakeIsGood = fake;
}

// Fake power being bad.
void voltageFakeIsBad(bool fake)
{
    gVoltageFakeIsBad = fake;
}

// End of file
