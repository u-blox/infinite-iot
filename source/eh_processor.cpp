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
#include <ble/GattCharacteristic.h> // for BLE UUIDs
#include <act_voltages.h> // For powerIsGood()
#include <eh_debug.h> // For LOG
#include <eh_utilities.h> // For ARRAY_SIZE
#include <eh_i2c.h>
#include <eh_config.h>
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
#include <ble_data_gather.h>
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

// A point to an event queue
static EventQueue *gpEventQueue = NULL;

/**************************************************************************
 * STATIC FUNCTIONS
 *************************************************************************/

// Check whether this thread has been terminated.
// Note: the signal is automagically reset after it has been received,
// hence it is necessary to pass in a pointer to the same "keepGoing" flag
// so that can be set in order to remember when the signal went south.
static bool threadContinue(bool *pKeepGoing)
{
    return (*pKeepGoing = *pKeepGoing && (Thread::signal_wait(TERMINATE_THREAD_SIGNAL, 0).status == osOK));
}

// Make a report.
static void doReport(Action *pAction, bool *pKeepGoing)
{
    MBED_ASSERT(pAction->type = ACTION_TYPE_REPORT);
    // TODO
}

// Get the time while making a report.
static void doGetTimeAndReport(Action *pAction, bool *pKeepGoing)
{
    MBED_ASSERT(pAction->type = ACTION_TYPE_GET_TIME_AND_REPORT);
    // TODO
}

// Measure humidity.
static void doMeasureHumidity(Action *pAction, bool *pKeepGoing)
{
    DataContents contents;

    MBED_ASSERT(pAction->type = ACTION_TYPE_MEASURE_HUMIDITY);
    // Make sure the device is up and take a measurement
    if (bme280Init(BME280_DEFAULT_ADDRESS) == ACTION_DRIVER_OK) {
        if (threadContinue(pKeepGoing) &&
            (getHumidity(&contents.humidity.percentage) == ACTION_DRIVER_OK)) {
            if (pDataAlloc(pAction, DATA_TYPE_HUMIDITY, 0, &contents) == NULL) {
                LOG(EVENT_DATA_ITEM_ALLOC_FAILURE, DATA_TYPE_HUMIDITY);
            }
        }
    } else {
        LOG(EVENT_ACTION_DRIVER_INIT_FAILURE, ACTION_TYPE_MEASURE_HUMIDITY);
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
    MBED_ASSERT(pAction->type = ACTION_TYPE_MEASURE_ATMOSPHERIC_PRESSURE);

    // Make sure the device is up and take a measurement
    if (bme280Init(BME280_DEFAULT_ADDRESS) == ACTION_DRIVER_OK) {
        if (threadContinue(pKeepGoing) &&
            (getPressure(&contents.atmosphericPressure.pascalX100) == ACTION_DRIVER_OK)) {
            if (pDataAlloc(pAction, DATA_TYPE_ATMOSPHERIC_PRESSURE, 0, &contents) == NULL) {
                LOG(EVENT_DATA_ITEM_ALLOC_FAILURE, DATA_TYPE_ATMOSPHERIC_PRESSURE);
            }
        }
    } else {
        LOG(EVENT_ACTION_DRIVER_INIT_FAILURE, ACTION_TYPE_MEASURE_ATMOSPHERIC_PRESSURE);
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
    MBED_ASSERT(pAction->type = ACTION_TYPE_MEASURE_TEMPERATURE);

    // Make sure the device is up and take a measurement
    if (bme280Init(BME280_DEFAULT_ADDRESS) == ACTION_DRIVER_OK) {
        if (threadContinue(pKeepGoing) &&
            (getTemperature(&contents.temperature.cX100) == ACTION_DRIVER_OK)) {
            if (pDataAlloc(pAction, DATA_TYPE_TEMPERATURE, 0, &contents) == NULL) {
                LOG(EVENT_DATA_ITEM_ALLOC_FAILURE, DATA_TYPE_TEMPERATURE);
            }
        }
    } else {
        LOG(EVENT_ACTION_DRIVER_INIT_FAILURE, ACTION_TYPE_MEASURE_TEMPERATURE);
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
    MBED_ASSERT(pAction->type = ACTION_TYPE_MEASURE_LIGHT);

    // Make sure the device is up and take a measurement
    if (si1133Init(SI1133_DEFAULT_ADDRESS) == ACTION_DRIVER_OK) {
        if (threadContinue(pKeepGoing) &&
            (getLight(&contents.light.lux, &contents.light.uvIndexX1000) == ACTION_DRIVER_OK)) {
            if (pDataAlloc(pAction, DATA_TYPE_LIGHT, 0, &contents) == NULL) {
                LOG(EVENT_DATA_ITEM_ALLOC_FAILURE, DATA_TYPE_LIGHT);
            }
        }
    } else {
        LOG(EVENT_ACTION_DRIVER_INIT_FAILURE, ACTION_TYPE_MEASURE_LIGHT);
    }
    // Shut the device down again
    si1133Deinit();
    // Done with this task now
    *pKeepGoing = false;
}

// Measure orientation.
static void doMeasureOrientation(Action *pAction, bool *pKeepGoing)
{
    DataContents contents;
    MBED_ASSERT(pAction->type = ACTION_TYPE_MEASURE_ORIENTATION);

    // No need to initialise orientation sensor, it's always on
    if (getOrientation(&contents.orientation.x, &contents.orientation.y,
                        &contents.orientation.z) == ACTION_DRIVER_OK) {
        if (pDataAlloc(pAction, DATA_TYPE_ORIENTATION, 0, &contents)) {
            LOG(EVENT_DATA_ITEM_ALLOC_FAILURE, DATA_TYPE_ORIENTATION);
        }
    }
    // Done with this task now
    *pKeepGoing = false;
}

// Measure position.
static void doMeasurePosition(Action *pAction, bool *pKeepGoing)
{
    DataContents contents;
    Timer timer;
    bool gotFix;
    MBED_ASSERT(pAction->type = ACTION_TYPE_MEASURE_POSITION);

    // Initialise the GNSS device and wait for a measurement
    // to pop-out.
    if (zoem8Init(ZOEM8_DEFAULT_ADDRESS) == ACTION_DRIVER_OK) {
        timer.start();
        while (threadContinue(pKeepGoing) &&
               !(gotFix = getPosition(&contents.position.latitudeX1000,
                                      &contents.position.longitudeX1000,
                                      &contents.position.radiusMetres,
                                      &contents.position.altitudeMetres,
                                      &contents.position.speedMPS) == ACTION_DRIVER_OK) &&
                (timer.read_ms() < POSITION_TIMEOUT_MS)) {
            wait_ms(POSITION_CHECK_INTERVAL_MS);
        }
        timer.stop();

        if (gotFix) {
            if (pDataAlloc(pAction, DATA_TYPE_POSITION, 0, &contents) == NULL) {
                LOG(EVENT_DATA_ITEM_ALLOC_FAILURE, DATA_TYPE_POSITION);
            }
        }
    } else {
        LOG(EVENT_ACTION_DRIVER_INIT_FAILURE, ACTION_TYPE_MEASURE_POSITION);
    }
    // Shut the device down again
    zoem8Deinit();
    // Done with this task now
    *pKeepGoing = false;
}

// Measure magnetic field strength.
static void doMeasureMagnetic(Action *pAction, bool *pKeepGoing)
{
    DataContents contents;
    MBED_ASSERT(pAction->type = ACTION_TYPE_MEASURE_MAGNETIC);

    // No need to initialise the Hall effect sensor, it's always on
    if (getFieldStrength(&contents.magnetic.teslaX1000) == ACTION_DRIVER_OK) {
        if (pDataAlloc(pAction, DATA_TYPE_MAGNETIC, 0, &contents) == NULL) {
            LOG(EVENT_DATA_ITEM_ALLOC_FAILURE, DATA_TYPE_MAGNETIC);
        }
    }
    // Done with this task now
    *pKeepGoing = false;
}

// Check on BLE progress
static void checkBleProgress(Action *pAction)
{
    DataContents contents;
    const char *pDeviceName;
    BleData *pBleData;
    int numDataItems;
    int numDevices = 0;
    int x;
    int nameLength = sizeof(contents.ble.name);

    // Check through all the BLE devices that have been found
    for (pDeviceName = pBleGetFirstDeviceName(); pDeviceName != NULL; pDeviceName = pBleGetNextDeviceName()) {
        numDevices++;
        numDataItems = bleGetNumDataItems(pDeviceName);
        if (numDataItems > 0) {
            // Retrieve any data items found for this device
            while ((pBleData = pBleGetFirstDataItem(pDeviceName, true)) != NULL) {
                x = strlen(pDeviceName);
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
}

// Read measurements from BLE devices.
static void doMeasureBle(Action *pAction, bool *pKeepGoing)
{
    Timer timer;
    int eventQueueId;
    MBED_ASSERT(pAction->type = ACTION_TYPE_MEASURE_BLE);
    MBED_ASSERT(gpEventQueue != NULL);

    bleInit(BLE_PEER_DEVICE_NAME_PREFIX, GattCharacteristic::UUID_BATTERY_LEVEL_STATE_CHAR, BLE_PEER_NUM_DATA_ITEMS, gpEventQueue, false);
    eventQueueId = gpEventQueue->call_every(PROCESSOR_IDLE_MS, callback(checkBleProgress, pAction));
    bleRun(BLE_ACTIVE_TIME_MS);
    timer.start();
    while (threadContinue(pKeepGoing) && (timer.read_ms() < BLE_ACTIVE_TIME_MS)) {
        wait_ms(PROCESSOR_IDLE_MS);
    }
    timer.stop();
    gpEventQueue->cancel(eventQueueId);
    bleDeinit();
    // Done with this task now
    *pKeepGoing = false;
}

// The callback that forms an action thread
static void doAction(Action *pAction)
{
    bool keepGoing = true;

    LOG(EVENT_ACTION_THREAD_STARTED, pAction->type);

    while (threadContinue(&keepGoing)) {

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

        if (gThreadDiagnosticsCallback) {
            keepGoing = gThreadDiagnosticsCallback(pAction);
        }
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
        LOG(EVENT_POWER, 1);

        // TODO determine wake-up reason
        LOG(EVENT_WAKE_UP, 0);

        // Rank the action log
        actionType = actionRankTypes();
        LOG(EVENT_ACTION, actionType);
        actionPrintRankedTypes();

        // Kick off actions while there's power and something to start
        while ((actionType != ACTION_TYPE_NULL) && voltageIsGood()) {
            // Get I2C going for the sensors
            i2cInit(I2C_SDA0, I2C_SCL0);
            // If there's an empty slot, start an action thread
            if (gpActionThreadList[taskIndex] == NULL) {
                pAction = pActionAdd(actionType);
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
        // more than one measurement
        bme280Deinit();

        // Shut down I2C
        i2cDeinit();

        LOG(EVENT_PROCESSOR_FINISHED, 0);
    }

    gpEventQueue = NULL;
}

// Set the thread diagnostics callback.
void processorSetThreadDiagnosticsCallback(Callback<bool(Action *)> threadDiagnosticsCallback)
{
    gThreadDiagnosticsCallback = threadDiagnosticsCallback;
}

// End of file
