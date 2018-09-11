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

#include <mbed.h> // For Threading and I2C pins
#include <log.h>
#include <act_voltages.h> // For voltageIsGood()
#include <eh_debug.h>
#include <eh_utilities.h> // For ARRAY_SIZE
#include <eh_i2c.h>
#include <eh_config.h>
#include <eh_statistics.h>
#include <act_cellular.h>
#include <act_modem.h>
#include <act_temperature_humidity_pressure.h>
#include <act_bme280.h>
#include <act_light.h>
#include <act_si1133.h>
#include <act_magnetic.h>
#include <act_si7210.h>
#include <act_acceleration.h>
#include <act_lis3dh.h>
#include <act_position.h>
#include <act_zoem8.h>
#if !MBED_CONF_APP_DISABLE_PERIPHERAL_HW && !defined (TARGET_UBLOX_C030_U201)
#include <ble/GattCharacteristic.h> // for BLE UUIDs
#include <ble_data_gather.h>
#endif
#include <eh_data.h>
#include <eh_processor.h>

/**************************************************************************
 * MANIFEST CONSTANTS
 *************************************************************************/

/** Set this signal to end a doAction() thread.
 */
#define TERMINATE_THREAD_SIGNAL 0x01

/** The main processing thread idles for this long when
 * waiting for the other threads to run.
 */
#define PROCESSOR_IDLE_MS 1000

/**************************************************************************
 * LOCAL VARIABLES
 *************************************************************************/

/** Flag so that we know if we've been initialised.
 */
static bool gInitialised = false;

/** Thread list.
 */
static Thread *gpActionThreadList[MAX_NUM_SIMULTANEOUS_ACTIONS];

/** Diagnostic hook.
 */
static Callback<bool(Action *)> gThreadDiagnosticsCallback;

/** A point to an event queue.
 */
static EventQueue *gpEventQueue = NULL;

/** The time at which logging was suspended.
 */
static time_t gLogSuspendTime;

/** The time at which time was last updated.
 */
static time_t gTimeUpdate;

/** The stack sizes required for each of the actions.
 */
static const int gStackSizes[] = {0,                                 // ACTION_TYPE_NULL
                                  4096,                              // ACTION_TYPE_REPORT
                                  4096,                              // ACTION_TYPE_GET_TIME_AND_REPORT
                                  ACTION_THREAD_STACK_SIZE_DEFAULT,  // ACTION_TYPE_MEASURE_HUMIDITY
                                  ACTION_THREAD_STACK_SIZE_DEFAULT,  // ACTION_TYPE_MEASURE_ATMOSPHERIC_PRESSURE
                                  ACTION_THREAD_STACK_SIZE_DEFAULT,  // ACTION_TYPE_MEASURE_TEMPERATURE
                                  ACTION_THREAD_STACK_SIZE_DEFAULT,  // ACTION_TYPE_MEASURE_LIGHT
                                  ACTION_THREAD_STACK_SIZE_DEFAULT,  // ACTION_TYPE_MEASURE_ACCELERATION
                                  ACTION_THREAD_STACK_SIZE_DEFAULT,  // ACTION_TYPE_MEASURE_MAGNETIC
                                  ACTION_THREAD_STACK_SIZE_DEFAULT}; // ACTION_TYPE_MEASURE_BLE

/**************************************************************************
 * STATIC FUNCTIONS
 *************************************************************************/

// Update the current time
static void updateTime(time_t timeUTC)
{
    statisticsTimeUpdate(timeUTC);
    set_time(timeUTC);
    gTimeUpdate = timeUTC;
    LOGX(EVENT_TIME_SET, timeUTC);
}

// Check that the required heap margin is available.
static bool heapIsAboveMargin(unsigned int margin)
{
    bool success = false;
    void *pMalloc;

    pMalloc = malloc(margin);
    if (pMalloc != NULL) {
        success = true;
        free(pMalloc);
    }

    return success;
}

// Check whether this thread has been terminated.
// Note: the signal is automagically reset after it has been received,
// hence it is necessary to pass in a pointer to the same "keepGoing" flag
// so that can be set in order to remember when the signal went south.
static bool threadContinue(bool *pKeepGoing)
{
    return (*pKeepGoing = *pKeepGoing && (Thread::signal_wait(TERMINATE_THREAD_SIGNAL, 0).status == osOK));
}

// Do both sorts of reporting (get time as an option).
static void reporting(Action *pAction, bool *pKeepGoing, bool getTime)
{
    DataContents contents;
    bool cellularMeasurementTaken = false;
    time_t timeUTC;
    char imeiString[MODEM_IMEI_LENGTH];
    int x;

    // Initialise the cellular modem
    if (modemInit(SIM_PIN, APN, USERNAME, PASSWORD) == ACTION_DRIVER_OK) {

        // Obtain the IMEI
        if (threadContinue(pKeepGoing)) {
            // Fill with something unique so that we know when an
            // error has occurred here
            memset (imeiString, '6', sizeof(imeiString));
            imeiString[sizeof(imeiString) - 1] = 0;
            if ((modemGetImei(imeiString) != ACTION_DRIVER_OK)) {
                // Carry on anyway, better to make a report with
                // an un-initialised IMEI
                LOGX(EVENT_GET_IMEI_FAILURE, 0);
            }
        }

        // Add the current statistics
        statisticsGet(&contents.statistics);
        if (pDataAlloc(NULL, DATA_TYPE_STATISTICS, 0, &contents) == NULL) {
            LOGX(EVENT_DATA_ITEM_ALLOC_FAILURE, DATA_TYPE_STATISTICS);
            LOGX(EVENT_DATA_CURRENT_SIZE_BYTES, dataGetBytesUsed());
        }

        // Collect the stored log entries
        dataLockList();
        while (dataAllocCheck(DATA_TYPE_LOG) &&
               ((contents.log.numItems = getLog(contents.log.log, ARRAY_SIZE(contents.log.log))) > 0)) {
            contents.log.logClientVersion = LOG_VERSION;
            contents.log.logApplicationVersion = APPLICATION_LOG_VERSION;
            // No point in logging the failure of a failure to allocate log space,
            // so don't check the return value here
            pDataAlloc(NULL, DATA_TYPE_LOG, 0, &contents);
        }
        dataUnlockList();

        // Make a connection
        if (threadContinue(pKeepGoing)) {
            if (modemConnect() == ACTION_DRIVER_OK) {
                // Get the time if required
                if (threadContinue(pKeepGoing) && getTime) {
                    if (modemGetTime(&timeUTC) == ACTION_DRIVER_OK) {
                        updateTime(timeUTC);
                    } else {
                        LOGX(EVENT_GET_TIME_FAILURE, 0);
                    }
                }
                // Get the cellular measurements
                if (threadContinue(pKeepGoing)) {
                    cellularMeasurementTaken |= (getCellularSignalRx(&contents.cellular.rsrpDbm,
                                                                     &contents.cellular.rssiDbm,
                                                                     &contents.cellular.rsrqDb,
                                                                     &contents.cellular.snrDb) == ACTION_DRIVER_OK);
                    cellularMeasurementTaken |= (getCellularSignalTx(&contents.cellular.transmitPowerDbm) == ACTION_DRIVER_OK);
                    cellularMeasurementTaken |= (getCellularChannel(&contents.cellular.cellId,
                                                                    &contents.cellular.earfcn,
                                                                    &contents.cellular.ecl) == ACTION_DRIVER_OK);
                    if (cellularMeasurementTaken) {
                        if (pDataAlloc(pAction, DATA_TYPE_CELLULAR, 0, &contents) == NULL) {
                            LOGX(EVENT_DATA_ITEM_ALLOC_FAILURE, DATA_TYPE_CELLULAR);
                            LOGX(EVENT_DATA_CURRENT_SIZE_BYTES, dataGetBytesUsed());
                        }
                    }
                }
                // Send reports
                if (threadContinue(pKeepGoing)) {
                    x = modemSendReports(IOT_SERVER_IP_ADDRESS, IOT_SERVER_PORT,
                                         imeiString);
                    if (x == ACTION_DRIVER_OK) {
                        actionCompleted(pAction);
                    } else {
                        LOGX(EVENT_SEND_FAILURE, x);
                    }
                }
            } else {
                LOGX(EVENT_CONNECT_FAILURE, modemGetLastConnectErrorCode());
            }
        }
    } else {
        LOGX(EVENT_ACTION_DRIVER_INIT_FAILURE, ACTION_TYPE_REPORT);
    }

    // Shut the modem down again
    modemDeinit();
    // Done with this task now
    *pKeepGoing = false;
}

// Make a report, if there is data to report.
static void doReport(Action *pAction, bool *pKeepGoing)
{
    int logEntryThreshold = 100;

    MBED_ASSERT(pAction->type == ACTION_TYPE_REPORT);

    // Set a threshold on the number of log entries
    // that will cause us to send that is not going to
    // be greater than MAX_NUM_LOG_ENTRIES less a percentage
    // to avoid overflow
    if (logEntryThreshold > MAX_NUM_LOG_ENTRIES * 9 / 10) {
        logEntryThreshold = MAX_NUM_LOG_ENTRIES * 9 / 10;
    }

    // Only bother if there is data to send
    // or there is a build-up of logging
    if ((dataCount() > 0) || (getNumLogEntries() > logEntryThreshold)) {
        reporting(pAction, pKeepGoing, false);
    }
    *pKeepGoing = false;
}

// Get the time while making a report.
static void doGetTimeAndReport(Action *pAction, bool *pKeepGoing)
{
    MBED_ASSERT(pAction->type == ACTION_TYPE_GET_TIME_AND_REPORT);

    reporting(pAction, pKeepGoing, true);
}

// Measure humidity.
static void doMeasureHumidity(Action *pAction, bool *pKeepGoing)
{
    DataContents contents;

    MBED_ASSERT(pAction->type == ACTION_TYPE_MEASURE_HUMIDITY);
    if (heapIsAboveMargin(MODEM_HEAP_REQUIRED_BYTES)) {
        // Make sure the device is up and take a measurement
        if (bme280Init(BME280_DEFAULT_ADDRESS) == ACTION_DRIVER_OK) {
            if (threadContinue(pKeepGoing) &&
                (getHumidity(&contents.humidity.percentage) == ACTION_DRIVER_OK)) {
                actionCompleted(pAction);
                if (pDataAlloc(pAction, DATA_TYPE_HUMIDITY, 0, &contents) == NULL) {
                    LOGX(EVENT_DATA_ITEM_ALLOC_FAILURE, DATA_TYPE_HUMIDITY);
                    LOGX(EVENT_DATA_CURRENT_SIZE_BYTES, dataGetBytesUsed());
                }
            }
        } else {
            LOGX(EVENT_ACTION_DRIVER_INIT_FAILURE, ACTION_TYPE_MEASURE_HUMIDITY);
        }
    } else {
        LOGX(EVENT_ACTION_DRIVER_HEAP_TOO_LOW, pAction->type);
    }

    // Don't deinitialise afterwards in case we are taking a
    // temperature or atmospheric pressure reading also
    // Done with this task now
    *pKeepGoing = false;
}

// Measure atmospheric pressure.
static void doMeasureAtmosphericPressure(Action *pAction, bool *pKeepGoing)
{
    DataContents contents;

    MBED_ASSERT(pAction->type == ACTION_TYPE_MEASURE_ATMOSPHERIC_PRESSURE);

    if (heapIsAboveMargin(MODEM_HEAP_REQUIRED_BYTES)) {
        // Make sure the device is up and take a measurement
        if (bme280Init(BME280_DEFAULT_ADDRESS) == ACTION_DRIVER_OK) {
            if (threadContinue(pKeepGoing) &&
                (getPressure(&contents.atmosphericPressure.pascalX100) == ACTION_DRIVER_OK)) {
                actionCompleted(pAction);
                if (pDataAlloc(pAction, DATA_TYPE_ATMOSPHERIC_PRESSURE, 0, &contents) == NULL) {
                    LOGX(EVENT_DATA_ITEM_ALLOC_FAILURE, DATA_TYPE_ATMOSPHERIC_PRESSURE);
                    LOGX(EVENT_DATA_CURRENT_SIZE_BYTES, dataGetBytesUsed());
                }
            }
        } else {
            LOGX(EVENT_ACTION_DRIVER_INIT_FAILURE, ACTION_TYPE_MEASURE_ATMOSPHERIC_PRESSURE);
        }
    } else {
        LOGX(EVENT_ACTION_DRIVER_HEAP_TOO_LOW, pAction->type);
    }

    // Don't deinitialise afterwards in case we are taking a
    // humidity or atmospheric pressure reading also
    // Done with this task now
    *pKeepGoing = false;
}

// Measure temperature.
static void doMeasureTemperature(Action *pAction, bool *pKeepGoing)
{
    DataContents contents;

    MBED_ASSERT(pAction->type == ACTION_TYPE_MEASURE_TEMPERATURE);

    if (heapIsAboveMargin(MODEM_HEAP_REQUIRED_BYTES)) {
        // Make sure the device is up and take a measurement
        if (bme280Init(BME280_DEFAULT_ADDRESS) == ACTION_DRIVER_OK) {
            if (threadContinue(pKeepGoing) &&
                (getTemperature(&contents.temperature.cX100) == ACTION_DRIVER_OK)) {
                actionCompleted(pAction);
                if (pDataAlloc(pAction, DATA_TYPE_TEMPERATURE, 0, &contents) == NULL) {
                    LOGX(EVENT_DATA_ITEM_ALLOC_FAILURE, DATA_TYPE_TEMPERATURE);
                    LOGX(EVENT_DATA_CURRENT_SIZE_BYTES, dataGetBytesUsed());
                }
            }
        } else {
            LOGX(EVENT_ACTION_DRIVER_INIT_FAILURE, ACTION_TYPE_MEASURE_TEMPERATURE);
        }
    } else {
        LOGX(EVENT_ACTION_DRIVER_HEAP_TOO_LOW, pAction->type);
    }

    // Don't deinitialise afterwards in case we are taking a
    // humidity or atmospheric pressure reading also
    // Done with this task now
    *pKeepGoing = false;
}

// Measure light.
static void doMeasureLight(Action *pAction, bool *pKeepGoing)
{
    DataContents contents;

    MBED_ASSERT(pAction->type == ACTION_TYPE_MEASURE_LIGHT);

    if (heapIsAboveMargin(MODEM_HEAP_REQUIRED_BYTES)) {
        // Make sure the device is up and take a measurement
        if (si1133Init(SI1133_DEFAULT_ADDRESS) == ACTION_DRIVER_OK) {
            if (threadContinue(pKeepGoing) &&
                (getLight(&contents.light.lux, &contents.light.uvIndexX1000) == ACTION_DRIVER_OK)) {
                actionCompleted(pAction);
                if (pDataAlloc(pAction, DATA_TYPE_LIGHT, 0, &contents) == NULL) {
                    LOGX(EVENT_DATA_ITEM_ALLOC_FAILURE, DATA_TYPE_LIGHT);
                    LOGX(EVENT_DATA_CURRENT_SIZE_BYTES, dataGetBytesUsed());
                }
            }
        } else {
            LOGX(EVENT_ACTION_DRIVER_INIT_FAILURE, ACTION_TYPE_MEASURE_LIGHT);
        }

        // Shut the device down again
        si1133Deinit();
    } else {
        LOGX(EVENT_ACTION_DRIVER_HEAP_TOO_LOW, pAction->type);
    }

    // Done with this task now
    *pKeepGoing = false;
}

// Measure acceleration.
static void doMeasureAcceleration(Action *pAction, bool *pKeepGoing)
{
    DataContents contents;

    MBED_ASSERT(pAction->type == ACTION_TYPE_MEASURE_ACCELERATION);

    if (heapIsAboveMargin(MODEM_HEAP_REQUIRED_BYTES)) {
        // No need to initialise acceleration sensor, it's always on
        if (getAcceleration(&contents.acceleration.xGX1000, &contents.acceleration.yGX1000,
                            &contents.acceleration.zGX1000) == ACTION_DRIVER_OK) {
            actionCompleted(pAction);
            if (pDataAlloc(pAction, DATA_TYPE_ACCELERATION, 0, &contents) == NULL) {
                LOGX(EVENT_DATA_ITEM_ALLOC_FAILURE, DATA_TYPE_ACCELERATION);
                LOGX(EVENT_DATA_CURRENT_SIZE_BYTES, dataGetBytesUsed());
            }
        }
    } else {
        LOGX(EVENT_ACTION_DRIVER_HEAP_TOO_LOW, pAction->type);
    }

    // Done with this task now
    *pKeepGoing = false;
}

// Measure position.
static void doMeasurePosition(Action *pAction, bool *pKeepGoing)
{
    DataContents contents;
    Timer timer;
    bool gotFix = false;
    unsigned char SVs = 0;
    time_t timeUTC;

    MBED_ASSERT(pAction->type == ACTION_TYPE_MEASURE_POSITION);

    if (heapIsAboveMargin(MODEM_HEAP_REQUIRED_BYTES)) {
        // Initialise the GNSS device and wait for a measurement
        // to pop-out.
        if (zoem8Init(ZOEM8_DEFAULT_ADDRESS) == ACTION_DRIVER_OK) {
            timer.reset();
            timer.start();
            statisticsIncPositionAttempts();
            while (threadContinue(pKeepGoing) &&
                   !(gotFix = (getPosition(&contents.position.latitudeX10e7,
                                           &contents.position.longitudeX10e7,
                                           &contents.position.radiusMetres,
                                           &contents.position.altitudeMetres,
                                           &contents.position.speedMPS,
                                           &SVs) == ACTION_DRIVER_OK)) &&
                    (timer.read_ms() < POSITION_TIMEOUT_MS)) {
                Thread::wait(POSITION_CHECK_INTERVAL_MS);
            }
            timer.stop();

            if (gotFix) {
                statisticsIncPositionSuccess();
                statisticsLastSVs(SVs);
                actionCompleted(pAction);
                if (pDataAlloc(pAction, DATA_TYPE_POSITION, 0, &contents) == NULL) {
                    LOGX(EVENT_DATA_ITEM_ALLOC_FAILURE, DATA_TYPE_POSITION);
                    LOGX(EVENT_DATA_CURRENT_SIZE_BYTES, dataGetBytesUsed());
                }
                // Since GNSS is able to get a fix, get the time also
                if (getTime(&timeUTC) == ACTION_DRIVER_OK) {
                    updateTime(timeUTC);
                }
            }
        } else {
            LOGX(EVENT_ACTION_DRIVER_INIT_FAILURE, ACTION_TYPE_MEASURE_POSITION);
        }
        // Shut the device down again
        zoem8Deinit();
    } else {
        LOGX(EVENT_ACTION_DRIVER_HEAP_TOO_LOW, pAction->type);
    }

    // Done with this task now
    *pKeepGoing = false;
}

// Measure magnetic field strength.
static void doMeasureMagnetic(Action *pAction, bool *pKeepGoing)
{
    DataContents contents;

    MBED_ASSERT(pAction->type == ACTION_TYPE_MEASURE_MAGNETIC);

    if (heapIsAboveMargin(MODEM_HEAP_REQUIRED_BYTES)) {
        // No need to initialise the Hall effect sensor, it's always on
        if (getFieldStrength(&contents.magnetic.teslaX1000) == ACTION_DRIVER_OK) {
            actionCompleted(pAction);
            if (pDataAlloc(pAction, DATA_TYPE_MAGNETIC, 0, &contents) == NULL) {
                LOGX(EVENT_DATA_ITEM_ALLOC_FAILURE, DATA_TYPE_MAGNETIC);
                LOGX(EVENT_DATA_CURRENT_SIZE_BYTES, dataGetBytesUsed());
            }
        }
    } else {
        LOGX(EVENT_ACTION_DRIVER_HEAP_TOO_LOW, pAction->type);
    }

    // Done with this task now
    *pKeepGoing = false;
}

// Check on BLE progress
static void checkBleProgress(Action *pAction)
{
    DataContents contents;
    const char *pDeviceName;
    int numDataItems;
    int numDevices = 0;
    int x;
    int nameLength = sizeof(contents.ble.name);
#if !MBED_CONF_APP_DISABLE_PERIPHERAL_HW && !defined (TARGET_UBLOX_C030_U201)
    BleData *pBleData;
#endif

#if !MBED_CONF_APP_DISABLE_PERIPHERAL_HW && !defined (TARGET_UBLOX_C030_U201)
    // Check through all the BLE devices that have been found
    for (pDeviceName = pBleGetFirstDeviceName(); pDeviceName != NULL; pDeviceName = pBleGetNextDeviceName()) {
        numDevices++;
        numDataItems = bleGetNumDataItems(pDeviceName);
        if (numDataItems > 0) {
            // Retrieve any data items found for this device
            while ((pBleData = pBleGetFirstDataItem(pDeviceName, true)) != NULL) {
                x = strlen(pDeviceName) + 1; // +1 for terminator
                if (x < nameLength) {
                    nameLength = x;
                }
                memcpy(&contents.ble.name, pDeviceName, nameLength);
                memcpy(&contents.ble.batteryPercentage, pBleData->pData, 1);
                if (pDataAlloc(pAction, DATA_TYPE_BLE, 0, &contents) == NULL) {
                    LOGX(EVENT_DATA_ITEM_ALLOC_FAILURE, DATA_TYPE_BLE);
                    LOGX(EVENT_DATA_CURRENT_SIZE_BYTES, dataGetBytesUsed());
                }
                free(pBleData->pData);
                free(pBleData);
            }
        }
    }
#endif
}

// Read measurements from BLE devices.
static void doMeasureBle(Action *pAction, bool *pKeepGoing)
{
    Timer timer;
    int eventQueueId;

    MBED_ASSERT(pAction->type == ACTION_TYPE_MEASURE_BLE);
    MBED_ASSERT(gpEventQueue != NULL);

#if !MBED_CONF_APP_DISABLE_PERIPHERAL_HW && !defined (TARGET_UBLOX_C030_U201)
    if (heapIsAboveMargin(MODEM_HEAP_REQUIRED_BYTES)) {
        bleInit(BLE_PEER_DEVICE_NAME_PREFIX, GattCharacteristic::UUID_BATTERY_LEVEL_STATE_CHAR, BLE_PEER_NUM_DATA_ITEMS, gpEventQueue, false);
        eventQueueId = gpEventQueue->call_every(PROCESSOR_IDLE_MS, callback(checkBleProgress, pAction));
        bleRun(BLE_ACTIVE_TIME_MS);
        timer.reset();
        timer.start();
        while (threadContinue(pKeepGoing) && (timer.read_ms() < BLE_ACTIVE_TIME_MS)) {
            Thread::wait(PROCESSOR_IDLE_MS);
        }
        timer.stop();

        actionCompleted(pAction);
        gpEventQueue->cancel(eventQueueId);
        bleDeinit();
    } else {
        LOGX(EVENT_ACTION_DRIVER_HEAP_TOO_LOW, pAction->type);
    }
#endif
    // Done with this task now
    *pKeepGoing = false;
}

// The callback that forms an action thread
static void doAction(Action *pAction)
{
    bool keepGoing = true;

    LOGX(EVENT_ACTION_THREAD_STARTED, pAction->type);
    statisticsAddAction(pAction->type);

    while (threadContinue(&keepGoing)) {

#if !MBED_CONF_APP_DISABLE_PERIPHERAL_HW
        // Do a thing and check the above condition frequently
        switch (pAction->type) {
            case ACTION_TYPE_REPORT:
                doReport(pAction, &keepGoing);
            break;
            case ACTION_TYPE_GET_TIME_AND_REPORT:
                doGetTimeAndReport(pAction, &keepGoing);
            break;
            case ACTION_TYPE_MEASURE_HUMIDITY:
                doMeasureHumidity(pAction, &keepGoing);
            break;
            case ACTION_TYPE_MEASURE_ATMOSPHERIC_PRESSURE:
                doMeasureAtmosphericPressure(pAction, &keepGoing);
            break;
            case ACTION_TYPE_MEASURE_TEMPERATURE:
                doMeasureTemperature(pAction, &keepGoing);
            break;
            case ACTION_TYPE_MEASURE_LIGHT:
                doMeasureLight(pAction, &keepGoing);
            break;
            case ACTION_TYPE_MEASURE_ACCELERATION:
                doMeasureAcceleration(pAction, &keepGoing);
            break;
            case ACTION_TYPE_MEASURE_POSITION:
                doMeasurePosition(pAction, &keepGoing);
            break;
            case ACTION_TYPE_MEASURE_MAGNETIC:
                doMeasureMagnetic(pAction, &keepGoing);
            break;
            case ACTION_TYPE_MEASURE_BLE:
                doMeasureBle(pAction, &keepGoing);
            break;
            default:
                MBED_ASSERT(false);
            break;
        }
#endif

        if (gThreadDiagnosticsCallback) {
            keepGoing = gThreadDiagnosticsCallback(pAction);
        }
    }

    // If the action is not completed, mark it as aborted
    if (!isActionCompleted(pAction)) {
        actionAborted(pAction);
    }

    // Whether successful or not, add any energy used to
    // the statistics
    statisticsAddEnergy(pAction->energyCostUWH);

    LOGX(EVENT_ACTION_THREAD_TERMINATED, pAction->type);
}

// Tidy up any threads that have terminated, returning
// the number still running.
static int checkThreadsRunning()
{
    int numThreadsRunning = 0;

    for (unsigned int x = 0; x < ARRAY_SIZE(gpActionThreadList); x++) {
        if (gpActionThreadList[x] != NULL) {
            if (gpActionThreadList[x]->get_state() ==  rtos::Thread::Deleted) {
                delete gpActionThreadList[x];
                gpActionThreadList[x] = NULL;
            } else {
                numThreadsRunning++;
            }
        }
    }

    return numThreadsRunning;
}

// Terminate all running threads.
static void terminateAllThreads()
{
    unsigned int x;

    // Set the terminate signal on all threads
    for (x = 0; x < ARRAY_SIZE(gpActionThreadList); x++) {
        if (gpActionThreadList[x] != NULL) {
            gpActionThreadList[x]->signal_set(TERMINATE_THREAD_SIGNAL);
            LOGX(EVENT_ACTION_THREAD_SIGNALLED, 0);
        }
    }

    // Wait for them all to end
    while ((x = checkThreadsRunning()) > 0) {
        Thread::wait(PROCESSOR_IDLE_MS);
        LOGX(EVENT_ACTION_THREADS_RUNNING, x);
    }

    LOGX(EVENT_ALL_THREADS_TERMINATED, 0);
}

// Determine the wake-up reason.
static WakeUpReason processorWakeUpReason()
{
    WakeUpReason wakeUpReason = WAKE_UP_RTC;
    DataContents contents;

    if (getFieldStrengthInterruptFlag()) {
        wakeUpReason = WAKE_UP_MAGNETIC;
        clearFieldStrengthInterruptFlag();
    }

    if (getAccelerationInterruptFlag()) {
       wakeUpReason = WAKE_UP_ACCELERATION;
       clearAccelerationInterruptFlag();
   }

    contents.wakeUpReason.reason = wakeUpReason;
    if (pDataAlloc(NULL, DATA_TYPE_WAKE_UP_REASON, 0, &contents) == NULL) {
        LOGX(EVENT_DATA_ITEM_ALLOC_FAILURE, DATA_TYPE_WAKE_UP_REASON);
        LOGX(EVENT_DATA_CURRENT_SIZE_BYTES, dataGetBytesUsed());
    }

    return wakeUpReason;
}

/**************************************************************************
 * PUBLIC FUNCTIONS
 *************************************************************************/

// Initialise the processing system
void processorInit()
{
    if (!gInitialised) {
        // Initialise the action thread list
        for (unsigned int x = 0; x < ARRAY_SIZE(gpActionThreadList); x++) {
            gpActionThreadList[x] = NULL;
        }

        gTimeUpdate = 0;
        // Suspend logging here; processorHandleWakeup()
        // is responsible for resuming it
        suspendLog();
        gLogSuspendTime =  time(NULL);
        gThreadDiagnosticsCallback = NULL;
    }

    gInitialised = true;
}

// Handle wake-up of the system, only returning when it is time to sleep once
// more
void processorHandleWakeup(EventQueue *pEventQueue)
{
    ActionType actionType;
    Action *pAction;
    osStatus taskStatus;
    unsigned int taskIndex = 0;

    // gpEventQueue is used here as a flag to see if
    // we're already running: if we are, don't run again
    if (gpEventQueue == NULL) {
        gpEventQueue = pEventQueue;
        resumeLog((unsigned int) ((time(NULL) - gLogSuspendTime) * 1000000));

        // TODO decide what power source to use next

        LOGX(EVENT_WAKE_UP, processorWakeUpReason());

        LOGX(EVENT_V_BAT_OK_READING_MV, getVBatOkMV());
        LOGX(EVENT_V_PRIMARY_READING_MV, getVPrimaryMV());
        LOGX(EVENT_V_IN_READING_MV, getVInMV());

        // If there is enough power to operate, perform some actions
        if (voltageIsGood()) {
            LOGX(EVENT_POWER, 1);

            statisticsWakeUp();

            // Rank the action log
            actionType = actionRankTypes();
            // If we have updated time recently then remove ACTION_TYPE_GET_TIME_AND_REPORT
            // from the list and move ACTION_TYPE_REPORT to the end so that we report things from
            // this wake-up straight away rather than leaving them sitting around until next
            // time, otherwise move ACTION_TYPE_GET_TIME_AND_REPORT to the end and delete
            // ACTION_TYPE_REPORT
            if ((gTimeUpdate != 0) && (time(NULL) - gTimeUpdate < TIME_UPDATE_INTERVAL_SECONDS)) {
                actionType = actionRankDelType(ACTION_TYPE_GET_TIME_AND_REPORT);
                actionType = actionRankMoveType(ACTION_TYPE_REPORT, MAX_NUM_ACTION_TYPES);
            } else {
                actionType = actionRankMoveType(ACTION_TYPE_GET_TIME_AND_REPORT, MAX_NUM_ACTION_TYPES);
                actionType = actionRankDelType(ACTION_TYPE_REPORT);
            }

            LOGX(EVENT_ACTION, actionType);

            // Kick off actions while there's power and something to start
            while ((actionType != ACTION_TYPE_NULL) && voltageIsGood()) {
                // Get I2C going for the sensors
                i2cInit(PIN_I2C_SDA, PIN_I2C_SCL);
                // If there's an empty slot, start an action thread
                if (gpActionThreadList[taskIndex] == NULL) {
                    pAction = pActionAdd(actionType);
                    if (pAction != NULL) {
                        gpActionThreadList[taskIndex] = new Thread(osPriorityNormal, gStackSizes[actionType]);
                        if (gpActionThreadList[taskIndex] != NULL) {
                            taskStatus = gpActionThreadList[taskIndex]->start(callback(doAction, pAction));
                            if (taskStatus != osOK) {
                                LOGX(EVENT_ACTION_THREAD_START_FAILURE, taskStatus);
                                delete gpActionThreadList[taskIndex];
                                gpActionThreadList[taskIndex] = NULL;
                            }
                            actionType = actionRankNextType();
                            LOGX(EVENT_ACTION, actionType);
                        } else {
                            LOGX(EVENT_ACTION_THREAD_ALLOC_FAILURE, 0);
                            Thread::wait(PROCESSOR_IDLE_MS); // Out of memory, need something to finish
                        }
                        debugPrintRamStats();
                    } else {
                        LOGX(EVENT_ACTION_ALLOC_FAILURE, 0);
                        Thread::wait(PROCESSOR_IDLE_MS); // Out of memory, need something to finish
                    }
                }

                taskIndex++;
                if (taskIndex >= ARRAY_SIZE(gpActionThreadList)) {
                    taskIndex = 0;
                    LOGX(EVENT_ACTION_THREADS_RUNNING, checkThreadsRunning());
                    Thread::wait(PROCESSOR_IDLE_MS); // Relax a little once we've set a batch off
                }

                // Check if any threads have ended
                checkThreadsRunning();
            }

            LOGX(EVENT_POWER, voltageIsGood());

            // If we've got here then either we've kicked off all the required actions or
            // power is no longer good.  While power is good, just do a background check on
            // the progress of the remaining actions.
            while (voltageIsGood() && (checkThreadsRunning() > 0)) {
                Thread::wait(PROCESSOR_IDLE_MS);
            }

            LOGX(EVENT_POWER, voltageIsGood());

            // We've now either done everything or power has gone.  If there are threads
            // still running, terminate them.
            terminateAllThreads();

            // The temperature/humidity/pressure sensor is de-initialised
            // here; it is otherwise left up to save time when using it for
            // more than one measurement type
            bme280Deinit();

            // Shut down I2C
            i2cDeinit();

            LOGX(EVENT_PROCESSOR_FINISHED, 0);
            statisticsSleep();
        }

        // If the modem is not working, so it's not possible to get logs
        // sent off the device, then uncomment the line below to get a
        // print-out of all of the log entries since the dawn of time
        // at each wake-up
        //printLog();
        suspendLog();
        gLogSuspendTime = time(NULL);

        gpEventQueue = NULL;
    }
}

// Set the thread diagnostics callback.
void processorSetThreadDiagnosticsCallback(Callback<bool(Action *)> threadDiagnosticsCallback)
{
    gThreadDiagnosticsCallback = threadDiagnosticsCallback;
}

// End of file
