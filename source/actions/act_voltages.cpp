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
// voltage in mV = (reading - 1536) / 13.275
#define READING_TO_MV(reading) ((((int) (reading)) - 1536) * 1000 / 13275)

/**************************************************************************
 * LOCAL VARIABLES
 *************************************************************************/

// Analogue input pin to measure VIN.
static AnalogIn gVIn(PIN_ANALOGUE_VIN);

// Analogueish input pin that is VBAT_OK.
static AnalogIn gVBatOk(PIN_ANALOGUE_VBAT_OK);

// Analogue input pin to measure VPRIMARY.
static AnalogIn gVPrimary(PIN_ANALOGUE_VPRIMARY);

// Digital pin that enables the voltage dividers
// on all of the above
static DigitalOut gEnableVoltageMeasurement(PIN_ENABLE_VOLTAGE_DIVIDERS, 0);

// Fake power is good.
static bool gVoltageFakeIsGood = false;

// Fake power is bad.
static bool gVoltageFakeIsBad = false;

/**************************************************************************
 * STATIC FUNCTIONS
 *************************************************************************/

/**************************************************************************
 * PUBLIC FUNCTIONS
 *************************************************************************/

// Get the value of VBAT_OK.
int getVBatOkMV()
{
    int reading;

    gEnableVoltageMeasurement = 1;
    Thread::wait(1);
    reading = READING_TO_MV(gVBatOk.read_u16());
    gEnableVoltageMeasurement = 0;

    return reading;
}

// Get the value of VIN.
int getVInMV()
{
    int reading;

    gEnableVoltageMeasurement = 1;
    Thread::wait(1);
    reading = READING_TO_MV(gVIn.read_u16());
    gEnableVoltageMeasurement = 0;

    return reading;
}

// Get the value of VPRIMARY.
int getVPrimaryMV()
{
    int reading;

    gEnableVoltageMeasurement = 1;
    Thread::wait(1);
    reading = READING_TO_MV(gVPrimary.read_u16());
    gEnableVoltageMeasurement = 0;

    return reading;
}

// Get the [optimistic] estimate of energy available.
unsigned int getEnergyOptimisticNWH()
{
    unsigned int energyNWH = 0;

    // The idea here is to make sure that the
    // processor tries to do things, even if it
    // may fail in doing so, since it will
    // be checking the voltageIsNotBad() threshold
    // in any case, so above a certain threshold (e.g.
    // 3.6V) then we just report the maximum.

    // TODO

    energyNWH = 0xFFFFFFFF;

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
