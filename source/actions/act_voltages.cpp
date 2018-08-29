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

/**************************************************************************
 * LOCAL VARIABLES
 *************************************************************************/

// Analogue input pin to measure VIN.
static AnalogIn gVIn(PIN_ANALOGUE_VIN);

// Analogueish input pin that is VBAT_OK.
static AnalogIn gVBatOk(PIN_ANALOGUE_VBAT_OK);

// Analogue input pin to measure VPRIMARY.
static AnalogIn gVPrimary(PIN_ANALOGUE_VPRIMARY);

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
    // TODO: full scale is currently 4.2 V
    // but will be 4.64 V on the new boards.
    return ((int) gVBatOk.read_u16()) * 4200 / 0xFFFF;
}

// Get the value of VIN.
int getVInMV()
{
    // Full scale is 4.64 V.
    return ((int) gVIn.read_u16()) * 4640 / 0xFFFF;
}

// Get the value of VPRIMARY.
int getVPrimaryMV()
{
    // Full scale is 4.64 V.
    return ((int) gVBatOk.read_u16()) * 4640 / 0xFFFF;
}

// Check if VBAT_SEC is good enough to run from
bool voltageIsGood()
{
    bool vBatOk = false;

    // 3.2 V is a good value for the VBAT_OK threshold.
    if (getVBatOkMV() > 3200) {
        vBatOk = true;
    }

    return (vBatOk || gVoltageFakeIsGood) && !gVoltageFakeIsBad;
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
