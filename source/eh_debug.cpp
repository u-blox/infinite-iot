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
#include <SerialWireOutput.h>
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
static DigitalOut gDebugLed(PIN_DEBUG_LED, 0);

#ifdef MBED_CONF_APP_ENABLE_RAM_STATS
// Storage for heap stats
static mbed_stats_heap_t gStatsHeap;

// Storage for stack stats
static mbed_stats_stack_t gStatsStack;
#endif

/**************************************************************************
 * STATIC FUNCTIONS
 *************************************************************************/

// Our own fatal error hook.
static void fatalErrorHook(const mbed_error_ctx *error_ctx)
{
    PRINTF("\n***************** FATAL ERROR **********************\n");
    if (error_ctx != NULL) {
        PRINTF("Error context (see mbed_error.h for explanation):\n");
        PRINTF("Error type %d, code %d in module of type %d.\n",
               MBED_GET_ERROR_TYPE(error_ctx->error_status),
               MBED_GET_ERROR_CODE(error_ctx->error_status),
               MBED_GET_ERROR_MODULE(error_ctx->error_status));
        PRINTF("Error address 0x%08x, value 0x%08x.\n",
               error_ctx->error_address, error_ctx->error_value);
        PRINTF("Thread ID 0x%08x, thread entry point 0x%08x.\n",
               error_ctx->thread_id, error_ctx->thread_entry_address);
        PRINTF("Thread stack size %d, initial/current stack pointer 0x%08x/0x%08x (difference %d).\n",
               error_ctx->thread_stack_size, error_ctx->thread_stack_mem,
               error_ctx->thread_current_sp, error_ctx->thread_current_sp - error_ctx->thread_stack_mem);
        PRINTF("Error value 0x%08x (%d).\n", error_ctx->error_value,
               error_ctx->error_value);
#ifdef MBED_CONF_PLATFORM_MAX_ERROR_FILENAME_LEN
        PRINTF("Filename %s, line %d.\n", error_ctx->error_filename,
                error_ctx->error_line_number);
#endif
        debugPrintRamStats();
        PRINTF("******** FURTHER MBED ERROR INFO MAY FOLLOW **********\n");
    }
}

/**************************************************************************
 * PUBLIC FUNCTIONS
 *************************************************************************/

#ifdef ENABLE_PRINTF_SWO
// Hook into the weak function and allow Serial Wire Output
namespace mbed {
    FileHandle *mbed_target_override_console(int)
    {
         static SerialWireOutput swo;
         return &swo;
    }
}
#endif

// Override mbed_die()
void mbed_die(void)
{
    // Flash SOS four times and then restart
    for (int x = 0; x < 5; x++) {
        for (int i = 0; i < 4; ++i) {
            gDebugLed = 1;
            Thread::wait(150);
            gDebugLed = 0;
            Thread::wait(150);
        }

        for (int i = 0; i < 4; ++i) {
            gDebugLed = 1;
            Thread::wait(400);
            gDebugLed = 0;
            Thread::wait(400);
        }
    }

    NVIC_SystemReset();
}

// Initialise debug.
void debugInit()
{
    // Initialise Morse, in case we need it
    morseInit(&gDebugLed);

    // Set our own error hook
    mbed_set_error_hook(fatalErrorHook);
}

// Pulse the debug LED for a number of milliseconds
void debugPulseLed(int milliseconds)
{
    if (!morseIsActive()) {
        gDebugLed = 1;
        Thread::wait(milliseconds);
        gDebugLed = 0;
        Thread::wait(PULSE_GAP_MS);
    }
}

// Victory LED pattern
void debugVictoryLed(int count)
{
    if (!morseIsActive()) {
        for (int x = 0; x < count; x++) {
            gDebugLed = 1;
            Thread::wait(VERY_SHORT_PULSE_MS);
            gDebugLed = 0;
            Thread::wait(VERY_SHORT_PULSE_MS);
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
void debugPrintRamStats()
{
#ifdef MBED_CONF_APP_ENABLE_RAM_STATS
    mbed_stats_heap_get(&gStatsHeap);
    mbed_stats_stack_get(&gStatsStack);

    PRINTF("Heap left: %d byte(s), minimum stack left %d byte(s).\n", gStatsHeap.reserved_size - gStatsHeap.current_size, gStatsStack.reserved_size - gStatsStack.max_size);
#endif
}

// Get the heap left
int debugGetHeapLeft()
{
    int heap = -1;

#ifdef MBED_CONF_APP_ENABLE_RAM_STATS
    mbed_stats_heap_get(&gStatsHeap);
    heap  = gStatsHeap.reserved_size - gStatsHeap.current_size;
#endif

    return heap;
}

// Get the minimum heap left
int debugGetHeapMinLeft()
{
    int heap = -1;

#ifdef MBED_CONF_APP_ENABLE_RAM_STATS
    mbed_stats_heap_get(&gStatsHeap);
    heap  = gStatsHeap.reserved_size - gStatsHeap.max_size;
#endif

    return heap;
}

// Get the minimum stack left
int debugGetStackMinLeft()
{
    int stack = -1;

#ifdef MBED_CONF_APP_ENABLE_RAM_STATS
    mbed_stats_stack_get(&gStatsStack);
    stack = gStatsStack.reserved_size - gStatsStack.max_size;
#endif

    return stack;
}

// End of file
