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
#include <mbed_events.h>
#include <mbed_stats.h> // For heap stats
#include <cmsis_os.h>   // For stack stats
#include <eh_utilities.h>
#include <eh_processor.h>
#include <eh_debug.h>

/* This code is intended to run on a UBLOX NINA-B1 module mounted on
 * the tec_eh energy harvesting/sensor board.  It should be built with
 * mbed target TARGET_UBLOX_EVK_NINA_B1.
 */

/**************************************************************************
 * MANIFEST CONSTANTS
 *************************************************************************/

// How frequently to wake-up to see if there is enough energy
// to do anything
#ifndef MBED_CONF_APP_WAKEUP_INTERVAL_MS
# define MBED_CONF_APP_WAKEUP_INTERVAL_MS 60000
#endif

/**************************************************************************
 * LOCAL VARIABLES
 *************************************************************************/

// Output pin to switch on power to the I2C sensors.
static DigitalOut gEnable1V8(NINA_B1_GPIO_29);

// Output pin to switch on power to cellular modem.
static DigitalOut gCdcEnable(NINA_B1_GPIO_28);

// Output pin to *signal* power to cellular.
static DigitalOut gCpOn(NINA_B1_GPIO_21);

// Output pin to reset cellular.
static DigitalOut gCpResetBar(NINA_B1_GPIO_20);

// Output pin to reset everything.
static DigitalOut gResetBar(NINA_B1_GPIO_27);

// Output pin to switch on energy source 1.
static DigitalOut gEnergySource1(NC);

// Output pin to switch on energy source 2.
static DigitalOut gEnergySource2(NC);

// Output pin to switch on energy source 3.
static DigitalOut gEnergySource3(NC);

// Input pin to detect VBAT_OK on the BQ25505 chip going low.
static DigitalIn gVBatOkBar(NINA_B1_GPIO_18);

// Input pin for hall effect sensor alert.
static InterruptIn gMagneticInt(NC);

// Input pin for inertial sensor event.
static InterruptIn gInertialInt(NC);

// Analogue input pin to measure VIN.
static AnalogIn gVIn(NC);

// Analogue input pin to measure VSTOR.
static AnalogIn gVStor(NC);

// Analogue input pin to measure VPRIMARY.
static AnalogIn gVPrimary(NC);

// Flag to indicate the type of modem that is attached
static bool gUseR4Modem = false;

// The wake-up event queue
static EventQueue gWakeUpEventQueue(/* event count */ 10 * EVENTS_EVENT_SIZE);

/**************************************************************************
 * STATIC FUNCTIONS
 *************************************************************************/

/**************************************************************************
 * PUBLIC FUNCTIONS
 *************************************************************************/

// Main
int main()
{
    // Initialise debug
    initDebug();
    
    // Nice long pulse at the start to make it clear we're running
    pulseDebugLed(1000);
    wait_ms(1000);
    
    // TODO Check what kind of modem is attached

    // Call processor directly to begin with
    handleWakeup();

    // Now start the timed callback
    gWakeUpEventQueue.call_every(MBED_CONF_APP_WAKEUP_INTERVAL_MS, handleWakeup);
    gWakeUpEventQueue.dispatch_forever();
}

// End of file
