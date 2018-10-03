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
#include <eh_watchdog.h>

/**************************************************************************
 * MANIFEST CONSTANTS
 *************************************************************************/

/**************************************************************************
 * LOCAL VARIABLES
 *************************************************************************/

static void (*gpInterruptCallback)(void);

/**************************************************************************
 * STATIC FUNCTIONS
 *************************************************************************/

/**************************************************************************
 * PUBLIC FUNCTIONS: see section 40 of the NRF52832 product specification.
 *************************************************************************/

// The IRQ handler
void WDT_IRQHandler(void)
{
#if TARGET_UBLOX_EVK_NINA_B1
    NRF_WDT->EVENTS_TIMEOUT = 0;
    if (gpInterruptCallback != NULL) {
        gpInterruptCallback();
    }
#endif
}

// Initialise the watchdog.
bool initWatchdog(int timeoutSeconds, void (*pInterruptCallback)(void))
{
    bool success = false;

    gpInterruptCallback = NULL;

#if TARGET_UBLOX_EVK_NINA_B1
    if ((NRF_WDT->RUNSTATUS & 0x01) == 0) {
        // No need to configure anything as the defaults are good
        // Set timeout value timeout [s] = ( CRV + 1 ) / 32768
        NRF_WDT->CRV = (timeoutSeconds * 32768) - 1;
        if (pInterruptCallback != NULL) {
            gpInterruptCallback = pInterruptCallback;
            NVIC_SetPriority(WDT_IRQn, 7);
            NVIC_ClearPendingIRQ(WDT_IRQn);
            NVIC_EnableIRQ(WDT_IRQn);
            NRF_WDT->INTENSET = 1;
        }
        NRF_WDT->TASKS_START = 0x01;
        success = true;
    }
#endif

    return success;
}

// Feed the watchdog.
void feedWatchdog()
{
#if TARGET_UBLOX_EVK_NINA_B1
    // Write in the magic value
    NRF_WDT->RR[0] = 0x6E524635;
#endif
}

// End of file
