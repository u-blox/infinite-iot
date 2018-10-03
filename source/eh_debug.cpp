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
#include <log.h>
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

// Base UICR register (0 to 31); the ones below this
// we leave alone as they seem to be set to non-default
// values already.
#define UICR_BASE_REGISTER 20

/**************************************************************************
 * LOCAL VARIABLES
 *************************************************************************/

// Debug LED
static DigitalOut gDebugLed(PIN_DEBUG_LED, 0);

#ifdef MBED_CONF_APP_ENABLE_RAM_STATS
// Storage for heap stats
static mbed_stats_heap_t gStatsHeap;
#endif

/**************************************************************************
 * STATIC FUNCTIONS
 *************************************************************************/

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
    LOGX(EVENT_MBED_DIE_CALLED, time(NULL));
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
void debugInit(mbed_error_hook_t fatalErrorCallback)
{
    // Initialise Morse, in case we need it
    morseInit(&gDebugLed);

    // Set our own error hook
    mbed_set_error_hook(fatalErrorCallback);
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
    unsigned int stack = 0;
    int numThreads;
    mbed_stats_stack_t *pStats;

#ifdef MBED_CONF_APP_ENABLE_RAM_STATS
    numThreads= osThreadGetCount();
    pStats = (mbed_stats_stack_t*) malloc(numThreads * sizeof(*pStats));
    if (pStats != NULL) {
        stack = 0x7FFFFF;

        numThreads = mbed_stats_stack_get_each(pStats, numThreads);
        for (int i = 0; i < numThreads; i++) {
            if (pStats[i].reserved_size - pStats[i].max_size < stack) {
                stack = pStats[i].reserved_size - pStats[i].max_size;
            }
        }
        free (pStats);
    }
#endif

    return stack;
}

// Printf() out some RAM stats.
void debugPrintRamStats()
{
#ifdef MBED_CONF_APP_ENABLE_RAM_STATS
    PRINTF("Heap left: %d byte(s), minimum stack left %d byte(s).\n", debugGetHeapLeft(), debugGetStackMinLeft());
#endif
}

// Write error information to non-volatile memory.
void debugWriteErrorNV(RestartReason reason,
                       time_t restartTime,
                       unsigned int lR,
                       const mbed_error_ctx *pErrorContext)
{
#if TARGET_UBLOX_EVK_NINA_B1
    // Wait for NVM to become ready
    while (!NRF_NVMC->READY);
    // Enable writing to NVM
    NRF_NVMC->CONFIG = 1;
    // Store the restart reason in register 0
    while (!NRF_NVMC->READY);
    NRF_UICR->CUSTOMER[UICR_BASE_REGISTER + 0] = reason;
    // Store the restart time in register 1
    while (!NRF_NVMC->READY);
    NRF_UICR->CUSTOMER[UICR_BASE_REGISTER + 1] = restartTime;
    // Store the link register value in register 2
    while (!NRF_NVMC->READY);
    NRF_UICR->CUSTOMER[UICR_BASE_REGISTER + 2] = lR;
    if (pErrorContext != NULL) {
        // Store the error status in register 3
        while (!NRF_NVMC->READY);
        NRF_UICR->CUSTOMER[UICR_BASE_REGISTER + 3] = pErrorContext->error_status;
        // Store the error address in register 4
        while (!NRF_NVMC->READY);
        NRF_UICR->CUSTOMER[UICR_BASE_REGISTER + 4] = pErrorContext->error_address;
        // Store the error value in register 5
        while (!NRF_NVMC->READY);
        NRF_UICR->CUSTOMER[UICR_BASE_REGISTER + 5] = pErrorContext->error_value;
        // Store the thread ID in register 6
        while (!NRF_NVMC->READY);
        NRF_UICR->CUSTOMER[UICR_BASE_REGISTER + 6] = pErrorContext->thread_id;
        // Store the thread entry address in register 7
        while (!NRF_NVMC->READY);
        NRF_UICR->CUSTOMER[UICR_BASE_REGISTER + 7] = pErrorContext->thread_entry_address;
        // Store the thread stack size in register 8
        while (!NRF_NVMC->READY);
        NRF_UICR->CUSTOMER[UICR_BASE_REGISTER + 8] = pErrorContext->thread_stack_size;
        // Store the initial stack pointer in register 9
        while (!NRF_NVMC->READY);
        NRF_UICR->CUSTOMER[UICR_BASE_REGISTER + 9] = pErrorContext->thread_stack_mem;
        // Store the current stack pointer in register 10
        while (!NRF_NVMC->READY);
        NRF_UICR->CUSTOMER[UICR_BASE_REGISTER + 10] = pErrorContext->thread_current_sp;
    }
    // Disable writing to NVM
    NRF_NVMC->CONFIG = 0;
#endif
}

// Read restart information from non-volatile memory.
RestartReason debugReadErrorNV(time_t *pRestartTime,
                               unsigned int *pLR,
                               mbed_error_ctx *pErrorContext)
{
    RestartReason restartReason = RESTART_REASON_UNKNOWN;
    unsigned int x;

#if TARGET_UBLOX_EVK_NINA_B1
    // Wait for NVM to become ready
    while (!NRF_NVMC->READY);
    // Enable reading from NVM
    NRF_NVMC->CONFIG = 0;
    // Read the restart reason in register 0
    x = NRF_UICR->CUSTOMER[UICR_BASE_REGISTER + 0];
    if (x == 0xFFFFFFFF) {
        restartReason = RESTART_REASON_NO_RESTART;
    } else {
        restartReason = (RestartReason) x;
        if (pRestartTime != NULL) {
            // Get the restart time from register 1
            *pRestartTime = NRF_UICR->CUSTOMER[UICR_BASE_REGISTER + 1];
        }
        if (pLR != NULL) {
            // Get the link register from register 2
            *pLR = NRF_UICR->CUSTOMER[UICR_BASE_REGISTER + 2];
        }
        if (pErrorContext != NULL) {
            // Read the error status from register 3
            pErrorContext->error_status = NRF_UICR->CUSTOMER[UICR_BASE_REGISTER + 3];
            // Read the error address from register 4
            pErrorContext->error_address = NRF_UICR->CUSTOMER[UICR_BASE_REGISTER + 4];
            // Read the error value from register 5
            pErrorContext->error_value = NRF_UICR->CUSTOMER[UICR_BASE_REGISTER + 5];
            // Read the thread ID from register 6
            pErrorContext->thread_id = NRF_UICR->CUSTOMER[UICR_BASE_REGISTER + 6];
            // Read the thread entry address from register 7
            pErrorContext->thread_entry_address = NRF_UICR->CUSTOMER[UICR_BASE_REGISTER + 7];
            // Read the thread stack size from register 8
            pErrorContext->thread_stack_size = NRF_UICR->CUSTOMER[UICR_BASE_REGISTER + 8];
            // Read the initial stack pointer from register 9
            pErrorContext->thread_stack_mem = NRF_UICR->CUSTOMER[UICR_BASE_REGISTER + 9];
            // Read the current stack pointer from register 10
            pErrorContext->thread_current_sp = NRF_UICR->CUSTOMER[UICR_BASE_REGISTER + 10];
        }
    }
#endif

    return restartReason;
}

// Reset the error information in non-volatile memory.
void debugResetErrorNV()
{
#if TARGET_UBLOX_EVK_NINA_B1
    // We only want to erase locations UICR_BASE_REGISTER
    // onwards so, just in case they are important,
    // read out the registers up to UICR_BASE_REGISTER
    // so that they can be written back again afterwards
    unsigned int *pSaved = (unsigned int *) malloc(UICR_BASE_REGISTER);
    if (pSaved != NULL) {
        // Wait for NVM to become ready
        while (!NRF_NVMC->READY);
        // Read out the registers up to UICR_BASE_REGISTER
        for (int x = 0; x < UICR_BASE_REGISTER; x++) {
            *(pSaved + x) = NRF_UICR->CUSTOMER[x];
        }
    }
    // Enable erasing NVM
    NRF_NVMC->CONFIG = 2;
    // Wait for NVM to become ready
    while (!NRF_NVMC->READY);
    // Erase UICR
    NRF_NVMC->ERASEUICR = 1;

    if (pSaved != NULL) {
        // Enable writing to NVM
        NRF_NVMC->CONFIG = 1;
        // Put back the registers we read out
        // Write the registers up to UICR_BASE_REGISTER
        for (int x = 0; x < UICR_BASE_REGISTER; x++) {
            // Wait for NVM to become ready
            while (!NRF_NVMC->READY);
            NRF_UICR->CUSTOMER[x] = *(pSaved + x);
        }
    }

    // Wait for NVM to become ready
    while (!NRF_NVMC->READY);
    // Disable writing to NVM
    NRF_NVMC->CONFIG = 0;
#endif
}

// End of file
