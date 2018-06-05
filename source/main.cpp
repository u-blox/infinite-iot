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
#include <compile_time.h>
#include <eh_utilities.h>
#include <eh_processor.h>
#include <eh_debug.h>
#include <eh_config.h>
#include <eh_post.h>

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

// Output pin to switch on power to the cellular modem.
static DigitalOut gEnableCdc(PIN_ENABLE_CDC);

// Output pin to *signal* power to the cellular mdoem.
static DigitalOut gCpOn(PIN_CP_ON);

// Output pin to reset the cellular modem.
static DigitalOut gCpResetBar(PIN_CP_RESET_BAR);

// Output pin to reset everything.
static DigitalOut gGResetBar(PIN_GRESET_BAR);

// Output pin to switch on energy source 1.
static DigitalOut gEnableEnergySource1(PIN_ENABLE_ENERGY_SOURCE_1);

// Output pin to switch on energy source 2.
static DigitalOut gEnableEnergySource2(PIN_ENABLE_ENERGY_SOURCE_2);

// Output pin to switch on energy source 3.
static DigitalOut gEnableEnergySource3(PIN_ENABLE_ENERGY_SOURCE_3);

// Input pin for hall effect sensor alert.
static InterruptIn gIntMagnetic(PIN_INT_MAGNETIC);

// Input pin for orientation sensor interrupt.
static InterruptIn gIntOrientation(PIN_INT_ORIENTATION);

// Flag to indicate the type of modem that is attached
static bool gUseR4Modem = false;

// The wake-up event queue
static EventQueue gWakeUpEventQueue(/* event count */ 10 * EVENTS_EVENT_SIZE);

// The logging buffer
static char loggingBuffer[LOG_STORE_SIZE];

/**************************************************************************
 * STATIC FUNCTIONS
 *************************************************************************/

/**************************************************************************
 * PUBLIC FUNCTIONS
 *************************************************************************/

// Main
int main()
{
    // Initialise one-time only stuff
    initLog(loggingBuffer);
    debugInit();
    actionInit();

    LOG(EVENT_BUILD_TIME_UNIX_FORMAT, __COMPILE_TIME_UNIX__);

    // Nice long pulse at the start to make it clear we're running
    debugPulseLed(1000);
    wait_ms(1000);

    // TODO Check what kind of modem is attached

    // Perform power-on self test
    if (post(false) == 0) {

        // Call processor directly to begin with
        processorHandleWakeup();

        // Now start the timed callback
        gWakeUpEventQueue.call_every(MBED_CONF_APP_WAKEUP_INTERVAL_MS, processorHandleWakeup);
        gWakeUpEventQueue.dispatch_forever();
    }

    deinitLog();
}

// End of file
