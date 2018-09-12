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
#include <log.h>
#include <compile_time.h>
#include <eh_utilities.h>
#include <eh_codec.h> // For protocol version
#include <eh_processor.h>
#include <eh_statistics.h>
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
// Note: if the wake up interval is greater than 71 minutes (0xFFFFFFFF
// microseconds) then the logging system will be unable to tell if the
// logging timestamp has wrapped.  Not a problem for the main application
// but may affect your view of the debug logs sent to the server.
#ifndef MBED_CONF_APP_WAKEUP_INTERVAL_MS
# define MBED_CONF_APP_WAKEUP_INTERVAL_MS 120000
#endif

/**************************************************************************
 * LOCAL VARIABLES
 *************************************************************************/

// The wake-up event queue
static EventQueue gWakeUpEventQueue(/* event count */ 10 * EVENTS_EVENT_SIZE);

// The logging buffer
static char gLoggingBuffer[LOG_STORE_SIZE];

// The reset output to everything.
static DigitalOut gReset(PIN_GRESET_BAR, 1);

/**************************************************************************
 * STATIC FUNCTIONS
 *************************************************************************/

// Set the initial state of several pins to minimise current draw
// and make sure that GPIOs 28 and 29 are not NFC pins.
static void setHwState()
{
    // Release the NFC pins through an NVM setting, if required
    if (NRF_UICR->NFCPINS) {
        // Wait for NVM to become ready
        while (!NRF_NVMC->READY);
        // Enable writing to NVM
        NRF_NVMC->CONFIG = 1;
        // Set NCF pins to be GPIOs in NVM
        NRF_UICR->NFCPINS = 0;
        // Disable writing to NVM
        NRF_NVMC->CONFIG = 0;
        // Now reset for the NVM changes to take effect
        NVIC_SystemReset();
    }

    // Use a direct call into the Nordic driver layer to set the
    // Tx and Rx pins to a default state which should prevent
    // current being drawn from them by the modem
    nrf_gpio_cfg(MDMTXD,
                 NRF_GPIO_PIN_DIR_OUTPUT,
                 NRF_GPIO_PIN_INPUT_DISCONNECT,
                 NRF_GPIO_PIN_NOPULL,
                 NRF_GPIO_PIN_S0D1,
                 NRF_GPIO_PIN_NOSENSE);

    nrf_gpio_cfg(MDMRXD,
                 NRF_GPIO_PIN_DIR_OUTPUT,
                 NRF_GPIO_PIN_INPUT_DISCONNECT,
                 NRF_GPIO_PIN_NOPULL,
                 NRF_GPIO_PIN_S0D1,
                 NRF_GPIO_PIN_NOSENSE);

    nrf_gpio_cfg(PIN_CP_ON,
                 NRF_GPIO_PIN_DIR_OUTPUT,
                 NRF_GPIO_PIN_INPUT_DISCONNECT,
                 NRF_GPIO_PIN_NOPULL,
                 NRF_GPIO_PIN_S0D1,
                 NRF_GPIO_PIN_NOSENSE);

    // Similarly, the I2C pins (see SCL_PIN_INIT_CONF in nrf_drv_twi.c)
    nrf_gpio_cfg(PIN_I2C_SDA,
                 NRF_GPIO_PIN_DIR_INPUT,
                 NRF_GPIO_PIN_INPUT_DISCONNECT,
                 NRF_GPIO_PIN_NOPULL,
                 NRF_GPIO_PIN_S0D1,
                 NRF_GPIO_PIN_NOSENSE);

    nrf_gpio_cfg(PIN_I2C_SCL,
                 NRF_GPIO_PIN_DIR_INPUT,
                 NRF_GPIO_PIN_INPUT_DISCONNECT,
                 NRF_GPIO_PIN_NOPULL,
                 NRF_GPIO_PIN_S0D1,
                 NRF_GPIO_PIN_NOSENSE);
}

/**************************************************************************
 * PUBLIC FUNCTIONS
 *************************************************************************/

// Main
int main()
{
    // Initialise one-time only stuff
    setHwState();
    initLog(gLoggingBuffer);
    debugInit();
    actionInit();
    statisticsInit();

    // Log some fundamentals
    LOGX(EVENT_SYSTEM_VERSION, SYSTEM_VERSION_INT);
    LOGX(EVENT_BUILD_TIME_UNIX_FORMAT, __COMPILE_TIME_UNIX__);
    LOGX(EVENT_PROTOCOL_VERSION, CODEC_PROTOCOL_VERSION);

    // Nice long pulse at the start to make it clear we're running
    // and at the same time pull the reset low
    gReset = 0;
    debugPulseLed(1000);
    Thread::wait(1000);
    gReset = 1;

    // Perform power-on self test, which includes
    // finding out what kind of modem is attached
    // TODO: decide whether to tolerate failure of sensors
    // in the POST operation or not
    if (post(true) == POST_RESULT_OK) {

        // To see the POST results from the log, uncomment the
        // following line
        printLog();

        // Initialise the processor
        processorInit();

        // Call processor directly to begin with
        processorHandleWakeup(&gWakeUpEventQueue);

        // Now start the timed callback
        gWakeUpEventQueue.call_every(MBED_CONF_APP_WAKEUP_INTERVAL_MS, callback(processorHandleWakeup, &gWakeUpEventQueue));
        gWakeUpEventQueue.dispatch_forever();
    }

    // Should never get here but, in case we do, deinit logging.
    deinitLog();
}

// End of file
