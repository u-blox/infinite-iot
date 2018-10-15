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
#include <act_energy_source.h>

/**************************************************************************
 * MANIFEST CONSTANTS
 *************************************************************************/

/**************************************************************************
 * LOCAL VARIABLES
 *************************************************************************/

// Digital output to switch on energy source 1
static DigitalOut gEnableEnergySource1(PIN_ENABLE_ENERGY_SOURCE_1, 0);

// Digital output to switch on energy source 2
static DigitalOut gEnableEnergySource2(PIN_ENABLE_ENERGY_SOURCE_2, 0);

// Digital output to switch on energy source 3
static DigitalOut gEnableEnergySource3(PIN_ENABLE_ENERGY_SOURCE_3, 0);

/**************************************************************************
 * STATIC FUNCTIONS
 *************************************************************************/

/**************************************************************************
 * PUBLIC FUNCTIONS
 *************************************************************************/

// Set the energy source.
void setEnergySource(unsigned char source)
{
    // Note: always do the one that is on
    // last so as never to accidentally
    // have two sources on at once
    switch (source) {
        case 0:
            gEnableEnergySource1 = 0;
            gEnableEnergySource2 = 0;
            gEnableEnergySource3 = 0;
        break;
        case 1:
            gEnableEnergySource2 = 0;
            gEnableEnergySource3 = 0;
            Thread::wait(1);
            gEnableEnergySource1 = 1;
        break;
        case 2:
            gEnableEnergySource1 = 0;
            gEnableEnergySource3 = 0;
            Thread::wait(1);
            gEnableEnergySource2 = 1;
        break;
        case 3:
            gEnableEnergySource1 = 0;
            gEnableEnergySource2 = 0;
            Thread::wait(1);
            gEnableEnergySource3 = 1;
        break;
        default:
            MBED_ASSERT (false);
        break;
    }
}

// Get the active energy source.
unsigned char getEnergySource()
{
    unsigned char energySource = 0;

    if (gEnableEnergySource1) {
        energySource = 1;
    } else if (gEnableEnergySource2) {
        energySource = 2;
    } else if (gEnableEnergySource3) {
        energySource = 3;
    }

    return energySource;
}

// End of file
