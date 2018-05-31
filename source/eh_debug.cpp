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
#include <eh_morse.h>
#include <eh_config.h>
#include <eh_debug.h>

/**************************************************************************
 * MANIFEST CONSTANTS
 *************************************************************************/

// Debug LED
#define LONG_PULSE_MS        500
#define SHORT_PULSE_MS       50
#define VERY_SHORT_PULSE_MS  35 // Don't set this any smaller as this is the smallest
                                // value where individual flashes are visible on a mobile
                                // phone video
#define PULSE_GAP_MS         250

/**************************************************************************
 * LOCAL VARIABLES
 *************************************************************************/

// Debug LED
static DigitalOut gDebugLedBar(PIN_DEBUG_LED_BAR, 1);

#ifdef MBED_CONF_APP_ENABLE_RAM_STATS
// Storage for heap stats
static mbed_stats_heap_t gStatsHeap;

// Storage for stack stats
static mbed_stats_stack_t gStatsStack;
#endif

/**************************************************************************
 * STATIC FUNCTIONS
 *************************************************************************/

/**************************************************************************
 * PUBLIC FUNCTIONS
 *************************************************************************/

// Initialise debug.
void debugInit()
{
    // Initialise Morse, in case we need it
    morseInit(&gDebugLedBar);
}

// Pulse the debug LED for a number of milliseconds
void debugPulseLed(int milliseconds)
{
    if (!morseIsActive()) {
        gDebugLedBar = 1;
        wait_ms(milliseconds);
        gDebugLedBar = 0;
        wait_ms(PULSE_GAP_MS);
    }
}

// Victory LED pattern
void debugVictoryLed(int count)
{
    if (!morseIsActive()) {
        for (int x = 0; x < count; x++) {
            gDebugLedBar = 1;
            wait_ms(VERY_SHORT_PULSE_MS);
            gDebugLedBar = 0;
            wait_ms(VERY_SHORT_PULSE_MS);
        }
    }
}

// Indicate that a debugBad thing has happened, where the thing
// is identified by the number of pulses
void debugBad(int pulses)
{
    if (!morseIsActive()) {
        for (int x = 0; x < pulses; x++) {
            debugPulseLed(LONG_PULSE_MS);
        }
    }
}

// Printf() out some RAM stats
#ifdef MBED_CONF_APP_ENABLE_RAM_STATS
void debugPrintRamStats()
{
    mbed_stats_heap_get(&gStatsHeap);
    mbed_stats_stack_get(&gStatsStack);

    PRINTF("Heap left: %d byte(s), stack left %d byte(s).\n", gStatsHeap.reserved_size - gStatsHeap.max_size, gStatsStack.reserved_size - gStatsStack.max_size);
#ifndef ENABLE_PRINTF
    morsePrintf("H %d S %d", gStatsHeap.reserved_size - gStatsHeap.max_size, gStatsStack.reserved_size - gStatsStack.max_size);
#endif
}
#endif

// End of file
