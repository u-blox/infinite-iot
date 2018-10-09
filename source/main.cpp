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
#include <cmsis_compiler.h> // For __STATIC_INLINE and __ASM
#include <log.h>
#include <compile_time.h>
#include <eh_utilities.h>
#include <eh_watchdog.h>
#include <act_voltages.h> // For voltageIsGood()
#include <act_energy_source.h> // For enableEnergySource()
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

/**************************************************************************
 * LOCAL VARIABLES
 *************************************************************************/

// The wake-up event queue
static EventQueue gWakeUpEventQueue(/* event count */ 10 * EVENTS_EVENT_SIZE);

// The logging buffer, in an uninitialised RAM area
#if defined(__CC_ARM) || (defined(__ARMCC_VERSION) && (__ARMCC_VERSION >= 6010050))
__attribute__ ((section(".bss.noinit"), zero_init))
static char gLoggingBuffer[LOG_STORE_SIZE];
#elif defined(__GNUC__)
__attribute__ ((section(".noinit")))
static char gLoggingBuffer[LOG_STORE_SIZE];
#elif defined(__ICCARM__)
static char gLoggingBuffer[LOG_STORE_SIZE] @ ".noinit";
#endif

// Buffer to hold the data we collect.
static int gDataBuffer[DATA_MAX_SIZE_WORDS];

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

// The watchdog callback, runs for up to two 36 kHz clock
// cycles before the device is reset.
static void watchdogCallback()
{
    LOGX(EVENT_RESTART, RESTART_REASON_WATCHDOG);
    LOGX(EVENT_RESTART_TIME, time(NULL));
    LOGX(EVENT_RESTART_LINK_REGISTER, (unsigned int) MBED_CALLER_ADDR());
}

// Our own fatal error hook.
static void fatalErrorCallback(const mbed_error_ctx *pErrorContext)
{
    LOGX(EVENT_RESTART, RESTART_REASON_FATAL_ERROR);
    LOGX(EVENT_RESTART_TIME, time(NULL));
    LOGX(EVENT_RESTART_LINK_REGISTER, (unsigned int) MBED_CALLER_ADDR());
    if (pErrorContext != NULL) {
        LOGX(EVENT_RESTART_FATAL_ERROR_TYPE,
             MBED_GET_ERROR_TYPE(pErrorContext->error_status));
        LOGX(EVENT_RESTART_FATAL_ERROR_CODE,
             MBED_GET_ERROR_CODE(pErrorContext->error_status));
        LOGX(EVENT_RESTART_FATAL_ERROR_MODULE,
             MBED_GET_ERROR_MODULE(pErrorContext->error_status));
        LOGX(EVENT_RESTART_FATAL_ERROR_VALUE,
             pErrorContext->error_value);
        LOGX(EVENT_RESTART_FATAL_ERROR_ADDRESS,
             pErrorContext->error_address);
        LOGX(EVENT_RESTART_FATAL_ERROR_THREAD_ID,
             pErrorContext->thread_id);
        LOGX(EVENT_RESTART_FATAL_ERROR_THREAD_ENTRY_ADDRESS,
             pErrorContext->thread_entry_address);
        LOGX(EVENT_RESTART_FATAL_ERROR_THREAD_STACK_SIZE,
             pErrorContext->thread_stack_size);
        LOGX(EVENT_RESTART_FATAL_ERROR_THREAD_STACK_MEM,
             pErrorContext->thread_stack_mem);
        LOGX(EVENT_RESTART_FATAL_ERROR_THREAD_CURRENT_SP,
             pErrorContext->thread_current_sp);
    }
    LOGX(EVENT_HEAP_LEFT, debugGetHeapLeft());
    LOGX(EVENT_STACK_MIN_LEFT, debugGetStackMinLeft());
}

/**************************************************************************
 * PUBLIC FUNCTIONS
 *************************************************************************/

// Main
int main()
{
    int vBatOk;
    bool energyIsGood;
    unsigned long long int energyAvailableNWH;

    // No retained real-time clock on this chip so set time to
    // zero to get it running
    set_time(0);

    // Initialise one-time only stuff
    initWatchdog(WATCHDOG_INTERVAL_SECONDS, watchdogCallback);
    setHwState();
    initLog(gLoggingBuffer);
    dataInit(gDataBuffer);
    debugInit(fatalErrorCallback);
    actionInit();
    statisticsInit();

    // Log some fundamentals
    LOGX(EVENT_SYSTEM_VERSION, SYSTEM_VERSION_INT);
    // Note: this will log the time that THIS file was
    // last built so, when doing a formal release, make sure
    // it is a clean build
    LOGX(EVENT_BUILD_TIME_UNIX_FORMAT, __COMPILE_TIME_UNIX__);
    LOGX(EVENT_PROTOCOL_VERSION, CODEC_PROTOCOL_VERSION);

    // Get energy from somewhere
    enableEnergySource(ENERGY_SOURCE_DEFAULT);

    // LED pulse at the start to make it clear we're running
    // and at the same time pull the reset line low
    // NOTE: these and the following debugPulseLed() timings
    // are relatively long; this is to allow the power to the
    // modem, which may have been powered before we started
    // for all we know, to drop properly otherwise it can
    // be left in a strange state (it is not connected
    // to the system-wide reset line)
    gReset = 0;
    debugPulseLed(1000);
    Thread::wait(2000);
    gReset = 1;

    // Wait for there to be enough power to run
    LOGX(EVENT_V_BAT_OK_READING_MV, getVBatOkMV());
#if defined(__CC_ARM) || (defined(__ARMCC_VERSION) && (__ARMCC_VERSION >= 6010050))
#pragma diag_suppress 1293  //  suppressing warning "assignment in condition" on ARMCC
#endif
    while (!(energyIsGood = voltageIsGood())) {
        LOGX(EVENT_WAITING_ENERGY, energyIsGood);
        vBatOk = getVBatOkMV();
        LOGX(EVENT_V_BAT_OK_READING_MV, vBatOk);
        LOGX(EVENT_CURRENT_TIME_UTC, time(NULL));
        Thread::wait(WAKEUP_INTERVAL_SECONDS * 1000);
        feedWatchdog();
    }

    LOGX(EVENT_POWER, voltageIsGood() + voltageIsNotBad() + voltageIsBearable());
    energyAvailableNWH = getEnergyAvailableNWH();
    if (energyAvailableNWH < 0xFFFFFFFF) {
        LOGX(EVENT_ENERGY_AVAILABLE_NWH, (unsigned int) energyAvailableNWH);
    } else {
        LOGX(EVENT_ENERGY_AVAILABLE_UWH, (unsigned int) (energyAvailableNWH / 1000));
    }

    // Second LED pulse to indicate we're go
    debugPulseLed(1000);

    // Perform power-on self test, which includes
    // finding out what kind of modem is attached
    if (post(true, &gWakeUpEventQueue, processorHandleWakeup) == POST_RESULT_OK) {

        // Initialise the processor
        processorInit();

        // Call processor directly to begin with
        processorHandleWakeup(&gWakeUpEventQueue);

        // Now start the timed callback
        gWakeUpEventQueue.call_every(WAKEUP_INTERVAL_SECONDS * 1000, callback(processorHandleWakeup, &gWakeUpEventQueue));
        gWakeUpEventQueue.dispatch_forever();
    }

    // For neatness, deinit logging
    deinitLog();

    // Reset and try again after a wait to let PRINTF leave the building
    Thread::wait(1000);
    NVIC_SystemReset();
}

// End of file
