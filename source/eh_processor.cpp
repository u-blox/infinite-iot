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
#include <act_voltages.h> // For powerIsGood()
#include <eh_debug.h> // For LOG
#include <eh_utilities.h> // For ARRAY_SIZE
#include <eh_i2c.h>
#include <eh_config.h>
#include <act_cellular.h>
#include <act_modem.h>
#include <act_temperature_humidity_pressure.h>
#include <act_bme280.h>
#include <act_light.h>
#include <act_si1133.h>
#include <act_magnetic.h>
#include <act_si7210.h>
#include <act_orientation.h>
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
static Callback<bool(Action *)> gThreadDiagnosticsCallback = NULL;

/** A point to an event queue.
 */
static EventQueue *gpEventQueue = NULL;

/** The time at which logging was suspended.
 */
static unsigned int gLogSuspendTime = 0;

/**************************************************************************
 * STATIC FUNCTIONS
 *************************************************************************/

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
    time_t timeUtc;
    char imeiString[MODEM_IMEI_LENGTH];

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
                LOG(EVENT_GET_IMEI_FAILURE, 0);
            }
        }
        // Get the cellular measurements
        if (threadContinue(pKeepGoing) &&
            (getSignalStrengthRx(&contents.cellular.rsrpDbm,
                                 &contents.cellular.rssiDbm,
                                 &contents.cellular.rsrq,
                                 &contents.cellular.snrDbm,
                                 &contents.cellular.eclDbm) == ACTION_DRIVER_OK) &&
             (getSignalStrengthTx(&contents.cellular.transmitPowerDbm) == ACTION_DRIVER_OK) &&
             (getChannel(&contents.cellular.physicalCellId,
                         &contents.cellular.pci,
                         &contents.cellular.earfcn) == ACTION_DRIVER_OK)) {
            if (pDataAlloc(pAction, DATA_TYPE_CELLULAR, 0, &contents) == NULL) {
                LOG(EVENT_DATA_ITEM_ALLOC_FAILURE, DATA_TYPE_CELLULAR);
            }
        }
        // Make a connection
        if (threadContinue(pKeepGoing)) {
            if (modemConnect() == ACTION_DRIVER_OK) {
                // Get the time if required
                if (threadContinue(pKeepGoing) && getTime) {
                    if (modemGetTime(&timeUtc) == ACTION_DRIVER_OK) {
                        set_time(timeUtc);
                        LOG(EVENT_TIME_SET, timeUtc);
                    } else {
                        LOG(EVENT_GET_TIME_FAILURE, 0);
                    }
                }
                // Send reports
                if (threadContinue(pKeepGoing)) {
                    if (modemSendReports(IOT_SERVER_IP_ADDRESS, IOT_SERVER_PORT,
                                         imeiString) == ACTION_DRIVER_OK) {
                        actionCompleted(pAction);
                    } else {
                        LOG(EVENT_SEND_FAILURE, 0);
                    }
                }
            } else {
                LOG(EVENT_CONNECT_FAILURE, 0);
            }
        }
    } else {
        LOG(EVENT_ACTION_DRIVER_INIT_FAILURE, ACTION_TYPE_REPORT);
    }

    // Shut the modem down again
    modemDeinit();
    // Done with this task now
    *pKeepGoing = false;
}

// Make a report.
static void doReport(Action *pAction, bool *pKeepGoing)
{
    MBED_ASSERT(pAction->type == ACTION_TYPE_REPORT);

    reporting(pAction, pKeepGoing, false);
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
                    LOG(EVENT_DATA_ITEM_ALLOC_FAILURE, DATA_TYPE_HUMIDITY);
                }
            }
        } else {
            LOG(EVENT_ACTION_DRIVER_INIT_FAILURE, ACTION_TYPE_MEASURE_HUMIDITY);
        }
    } else {
        LOG(EVENT_ACTION_DRIVER_HEAP_TOO_LOW, MODEM_HEAP_REQUIRED_BYTES);
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
                    LOG(EVENT_DATA_ITEM_ALLOC_FAILURE, DATA_TYPE_ATMOSPHERIC_PRESSURE);
                }
            }
        } else {
            LOG(EVENT_ACTION_DRIVER_INIT_FAILURE, ACTION_TYPE_MEASURE_ATMOSPHERIC_PRESSURE);
        }
    } else {
        LOG(EVENT_ACTION_DRIVER_HEAP_TOO_LOW, MODEM_HEAP_REQUIRED_BYTES);
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
                    LOG(EVENT_DATA_ITEM_ALLOC_FAILURE, DATA_TYPE_TEMPERATURE);
                }
            }
        } else {
            LOG(EVENT_ACTION_DRIVER_INIT_FAILURE, ACTION_TYPE_MEASURE_TEMPERATURE);
        }
    } else {
        LOG(EVENT_ACTION_DRIVER_HEAP_TOO_LOW, MODEM_HEAP_REQUIRED_BYTES);
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
                    LOG(EVENT_DATA_ITEM_ALLOC_FAILURE, DATA_TYPE_LIGHT);
                }
            }
        } else {
            LOG(EVENT_ACTION_DRIVER_INIT_FAILURE, ACTION_TYPE_MEASURE_LIGHT);
        }

        // Shut the device down again
        si1133Deinit();
    } else {
        LOG(EVENT_ACTION_DRIVER_HEAP_TOO_LOW, MODEM_HEAP_REQUIRED_BYTES);
    }

    // Done with this task now
    *pKeepGoing = false;
}

// Measure orientation.
static void doMeasureOrientation(Action *pAction, bool *pKeepGoing)
{
    DataContents contents;

    MBED_ASSERT(pAction->type == ACTION_TYPE_MEASURE_ORIENTATION);

    if (heapIsAboveMargin(MODEM_HEAP_REQUIRED_BYTES)) {
        // No need to initialise orientation sensor, it's always on
        if (getOrientation(&contents.orientation.x, &contents.orientation.y,
                            &contents.orientation.z) == ACTION_DRIVER_OK) {
            actionCompleted(pAction);
            if (pDataAlloc(pAction, DATA_TYPE_ORIENTATION, 0, &contents)) {
                LOG(EVENT_DATA_ITEM_ALLOC_FAILURE, DATA_TYPE_ORIENTATION);
            }
        }
    } else {
        LOG(EVENT_ACTION_DRIVER_HEAP_TOO_LOW, MODEM_HEAP_REQUIRED_BYTES);
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

    MBED_ASSERT(pAction->type == ACTION_TYPE_MEASURE_POSITION);

    if (heapIsAboveMargin(MODEM_HEAP_REQUIRED_BYTES)) {
        // Initialise the GNSS device and wait for a measurement
        // to pop-out.
        if (zoem8Init(ZOEM8_DEFAULT_ADDRESS) == ACTION_DRIVER_OK) {
            timer.reset();
            timer.start();
            while (threadContinue(pKeepGoing) &&
                   !(gotFix = (getPosition(&contents.position.latitudeX10e7,
                                           &contents.position.longitudeX10e7,
                                           &contents.position.radiusMetres,
                                           &contents.position.altitudeMetres,
                                           &contents.position.speedMPS) == ACTION_DRIVER_OK)) &&
                    (timer.read_ms() < POSITION_TIMEOUT_MS)) {
                wait_ms(POSITION_CHECK_INTERVAL_MS);
            }
            timer.stop();

            if (gotFix) {
                actionCompleted(pAction);
                if (pDataAlloc(pAction, DATA_TYPE_POSITION, 0, &contents) == NULL) {
                    LOG(EVENT_DATA_ITEM_ALLOC_FAILURE, DATA_TYPE_POSITION);
                }
            }
        } else {
            LOG(EVENT_ACTION_DRIVER_INIT_FAILURE, ACTION_TYPE_MEASURE_POSITION);
        }
        // Shut the device down again
        zoem8Deinit();
    } else {
        LOG(EVENT_ACTION_DRIVER_HEAP_TOO_LOW, MODEM_HEAP_REQUIRED_BYTES);
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
                LOG(EVENT_DATA_ITEM_ALLOC_FAILURE, DATA_TYPE_MAGNETIC);
            }
        }
    } else {
        LOG(EVENT_ACTION_DRIVER_HEAP_TOO_LOW, MODEM_HEAP_REQUIRED_BYTES);
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
                    LOG(EVENT_DATA_ITEM_ALLOC_FAILURE, DATA_TYPE_BLE);
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

    MBED_ASSERT(pAction->type = ACTION_TYPE_MEASURE_BLE);
    MBED_ASSERT(gpEventQueue != NULL);

#if !MBED_CONF_APP_DISABLE_PERIPHERAL_HW && !defined (TARGET_UBLOX_C030_U201)
    if (heapIsAboveMargin(MODEM_HEAP_REQUIRED_BYTES)) {
        bleInit(BLE_PEER_DEVICE_NAME_PREFIX, GattCharacteristic::UUID_BATTERY_LEVEL_STATE_CHAR, BLE_PEER_NUM_DATA_ITEMS, gpEventQueue, false);
        eventQueueId = gpEventQueue->call_every(PROCESSOR_IDLE_MS, callback(checkBleProgress, pAction));
        bleRun(BLE_ACTIVE_TIME_MS);
        timer.reset();
        timer.start();
        while (threadContinue(pKeepGoing) && (timer.read_ms() < BLE_ACTIVE_TIME_MS)) {
            wait_ms(PROCESSOR_IDLE_MS);
        }
        timer.stop();

        actionCompleted(pAction);
        gpEventQueue->cancel(eventQueueId);
        bleDeinit();
    } else {
        LOG(EVENT_ACTION_DRIVER_HEAP_TOO_LOW, MODEM_HEAP_REQUIRED_BYTES);
    }
#endif
    // Done with this task now
    *pKeepGoing = false;
}

// The callback that forms an action thread
static void doAction(Action *pAction)
{
    bool keepGoing = true;

    LOG(EVENT_ACTION_THREAD_STARTED, pAction->type);

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
            case ACTION_TYPE_MEASURE_ORIENTATION:
                doMeasureOrientation(pAction, &keepGoing);
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

    LOG(EVENT_ACTION_THREAD_TERMINATED, pAction->type);
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
            LOG(EVENT_ACTION_THREAD_SIGNALLED, 0);
        }
    }

    // Wait for them all to end
    while ((x = checkThreadsRunning()) > 0) {
        wait_ms(PROCESSOR_IDLE_MS);
        LOG(EVENT_ACTION_THREADS_RUNNING, x);
    }

    LOG(EVENT_ALL_THREADS_TERMINATED, 0);
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

    gpEventQueue = pEventQueue;

    // TODO decide what power source to use next

    // If there is enough power to operate, perform some actions
    if (voltageIsGood()) {
        if (gLogSuspendTime != 0) {
            resumeLog(((unsigned int) (time(NULL)) - gLogSuspendTime) * 1000);
        }
        LOG(EVENT_POWER, 1);

        // TODO determine wake-up reason
        LOG(EVENT_WAKE_UP, 0);

        // Rank the action log
        actionType = actionRankTypes();
        LOG(EVENT_ACTION, actionType);

        // Kick off actions while there's power and something to start
        while ((actionType != ACTION_TYPE_NULL) && voltageIsGood()) {
            // Get I2C going for the sensors
            i2cInit(PIN_I2C_SDA, PIN_I2C_SCL);
            // If there's an empty slot, start an action thread
            if (gpActionThreadList[taskIndex] == NULL) {
                pAction = pActionAdd(actionType);
                if (pAction != NULL) {
                    gpActionThreadList[taskIndex] = new Thread(osPriorityNormal, ACTION_THREAD_STACK_SIZE);
                    if (gpActionThreadList[taskIndex] != NULL) {
                        taskStatus = gpActionThreadList[taskIndex]->start(callback(doAction, pAction));
                        if (taskStatus != osOK) {
                            LOG(EVENT_ACTION_THREAD_START_FAILURE, taskStatus);
                            delete gpActionThreadList[taskIndex];
                            gpActionThreadList[taskIndex] = NULL;
                        }
                        actionType = actionNextType();
                        LOG(EVENT_ACTION, actionType);
                    } else {
                        LOG(EVENT_ACTION_THREAD_ALLOC_FAILURE, 0);
                        wait_ms(PROCESSOR_IDLE_MS); // Out of memory, need something to finish
                    }
                } else {
                    LOG(EVENT_ACTION_ALLOC_FAILURE, 0);
                    wait_ms(PROCESSOR_IDLE_MS); // Out of memory, need something to finish
                }
            }

            taskIndex++;
            if (taskIndex >= ARRAY_SIZE(gpActionThreadList)) {
                taskIndex = 0;
                LOG(EVENT_ACTION_THREADS_RUNNING, checkThreadsRunning());
                wait_ms(PROCESSOR_IDLE_MS); // Relax a little once we've set a batch off
            }

            // Check if any threads have ended
            checkThreadsRunning();
        }

        LOG(EVENT_POWER, voltageIsGood());

        // If we've got here then either we've kicked off all the required actions or
        // power is no longer good.  While power is good, just do a background check on
        // the progress of the remaining actions.
        while (voltageIsGood() && (checkThreadsRunning() > 0)) {
            wait_ms(PROCESSOR_IDLE_MS);
        }

        LOG(EVENT_POWER, voltageIsGood());

        // We've now either done everything or power has gone.  If there are threads
        // still running, terminate them.
        terminateAllThreads();

        // The temperature/humidity/pressure sensor is de-initialised
        // here; it is otherwise left up to save time when using it for
        // more than one measurement type
        bme280Deinit();

        // Shut down I2C
        i2cDeinit();

        LOG(EVENT_PROCESSOR_FINISHED, 0);
        suspendLog();
        gLogSuspendTime = time(NULL);
    }

    gpEventQueue = NULL;
}

// Set the thread diagnostics callback.
void processorSetThreadDiagnosticsCallback(Callback<bool(Action *)> threadDiagnosticsCallback)
{
    gThreadDiagnosticsCallback = threadDiagnosticsCallback;
}

// End of file
