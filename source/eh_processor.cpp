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
#include <act_voltages.h> // For voltageIsGood() and voltageIsNotBad()
#include <act_energy_source.h>
#include <eh_debug.h>
#include <eh_utilities.h> // For ARRAY_SIZE and MTX_LOCK/MTX_UNLOCK
#include <eh_watchdog.h>
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
#include <act_energy_source.h>
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

/** Choose the current energy source (in gEnergyChoice[0]).
 */
#define CHOOSE_ENERGY_SOURCE(x) (gEnergyChoice[0] = (x) << 4)

/** Get the energy source from an entry in gEnergyChoice[].
 */
#define GET_ENERGY_SOURCE(x) ((x) >> 4)

/** Get whether the given energy source was good.
 */
#define GET_ENERGY_SOURCE_GOOD(x) (((x) & 0x01) > 0 ? true : false)

/** Mark the current energy source (in gEnergyChoice[0]) as good.
 */
#define SET_CURRENT_ENERGY_SOURCE_GOOD (gEnergyChoice[0] |= 0x01)

#if defined (MBED_CONFIG_APP_DISABLE_ENERGY_CHOOSER) && \
    MBED_CONFIG_APP_DISABLE_ENERGY_CHOOSER
#define DISABLE_ENERGY_CHOOSER
#endif

/**************************************************************************
 * LOCAL VARIABLES
 *************************************************************************/

/** Flag so that we know if we've been initialised.
 */
static bool gInitialised = false;

/** Flag to know if this is the first wake-up.
 */
static bool gJustBooted = true;

/** Thread list.
 */
static Thread *gpActionThreadList[MAX_NUM_SIMULTANEOUS_ACTIONS];

/** Diagnostic hook.
 */
static Callback<bool(Action *)> gThreadDiagnosticsCallback;

/** A point to an event queue.
 */
static EventQueue *gpEventQueue = NULL;

/** A general purpose mutex, used while fiddling with time
 * and energy calculations which we don't want affected
 * by threading clashes.
 */
static Mutex gMtx;

/** The time at which logging was suspended.
 */
static time_t gLogSuspendTime;

/** The log index to encode into a log data entry.
 */
static unsigned int gLogIndex;

/** The time at which time was last updated.
 */
static time_t gTimeUpdate;

/** Timer to keep track of how long we've been
 * actively processing.
 */
static Timer *gpProcessTimer = NULL;

/** A timer to keep track of when BLE is active.
 */
static Timer *gpBleTimer = NULL;

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
                                  ACTION_THREAD_STACK_SIZE_DEFAULT,  // ACTION_TYPE_MEASURE_POSITION
                                  ACTION_THREAD_STACK_SIZE_DEFAULT,  // ACTION_TYPE_MEASURE_MAGNETIC
                                  ACTION_THREAD_STACK_SIZE_DEFAULT}; // ACTION_TYPE_MEASURE_BLE

/** The data types produced by each action type.
 */
static const DataType gDataType[] = {DATA_TYPE_NULL,                 // ACTION_TYPE_NULL
                                     DATA_TYPE_NULL,                 // ACTION_TYPE_REPORT
                                     DATA_TYPE_NULL,                 // ACTION_TYPE_GET_TIME_AND_REPORT
                                     DATA_TYPE_HUMIDITY,             // ACTION_TYPE_MEASURE_HUMIDITY
                                     DATA_TYPE_ATMOSPHERIC_PRESSURE, // ACTION_TYPE_MEASURE_ATMOSPHERIC_PRESSURE
                                     DATA_TYPE_TEMPERATURE,          // ACTION_TYPE_MEASURE_TEMPERATURE
                                     DATA_TYPE_LIGHT,                // ACTION_TYPE_MEASURE_LIGHT
                                     DATA_TYPE_ACCELERATION,         // ACTION_TYPE_MEASURE_ACCELERATION
                                     DATA_TYPE_POSITION,             // ACTION_TYPE_MEASURE_POSITION
                                     DATA_TYPE_MAGNETIC,             // ACTION_TYPE_MEASURE_MAGNETIC
                                     DATA_TYPE_BLE};                 // ACTION_TYPE_MEASURE_BLE

/** Keep track of the energy cost of measurements for
 * the always-on BME280.
 */
static time_t gLastMeasurementTimeBme280Seconds;

/** Keep track of the energy cost of measurements for
 * the always-on LIS3DH.
 */
static time_t gLastMeasurementTimeLis3dhSeconds;

/** Keep track of the energy cost of measurements for
 * the always-on SI7210.
 */
static time_t gLastMeasurementTimeSi7210Seconds;

/** Keep track of the energy cost of measurements for
 * the always-on SI1133.
 */
static time_t gLastMeasurementTimeSi1133Seconds;

/** Keep track of the energy cost of measurements for
 * the modem in idle.
 */
static time_t gLastSleepTimeModemSeconds;

/** Keep track of the energy cost of BLE.
 */
static time_t gLastMeasurementTimeBleSeconds;

/** Keep track of the energy cost of the modem.
 */
static unsigned long long int gLastModemEnergyNWH;

/** Keep track of the portion of system idle energy to
 * add to each action's energy cost.
 */
static unsigned long long int gSystemIdleEnergyPropNWH;

/** Keep track of the portion of system active energy that
 * has already been allocated to actions.
 */
static unsigned long long int gSystemActiveEnergyAllocatedNWH;

/** Keep track of the portion of BLE active energy that
 * has already been allocated to BLE readings.
 */
static unsigned long long int gBleActiveEnergyAllocatedNWH;

/** Counter of ticks, believe it or not.
 */
static unsigned int gAwakeCount;

/** Array to keep track of the last N choices of
 * energy source and its success in obtaining enough
 * energy to wake-up again afterwards.  Coding is that
 * the upper-nibble is the energy source enum and the
 * lower nibble is 0 for failure or 1 for success.
 */
static unsigned char gEnergyChoice[ENERGY_HISTORY_SECONDS / WAKEUP_INTERVAL_SECONDS];

/** Array to keep track of VIN measurements.
 */
static int gVIn[ENERGY_SOURCES_MAX_NUM];

/** Array to keep track of how many VIN measurements are
 * in gVIn.
 */
static unsigned int gVInCount;

/** Track the number of wake-ups.
 */
static unsigned int gNumWakeups;

/** Track the number of wake-ups with enough energy
 * to run properly.
 */
static unsigned int gNumEnergeticWakeups;

/** The number of wake-ups to skip attempting a GNSS fix.
 */
static unsigned int gPositionFixSkipsRequired;

/** Keep a track of the current number of GNSS skips.
 */
static unsigned int gPositionNumFixesSkipped;

/** Keep a track of the number of GNSS tried and failed
 * without backing off.
 */
static unsigned int gPositionNumFixesFailedNoBackOff;

/** Keep track of whether the modem was idle between
 * wake-ups or was off and needed to register.
 */
static bool gModemOff;

/** Keep track of the number of failures to do a report.
 */
static unsigned int gReportNumFailures;

/**************************************************************************
 * STATIC FUNCTIONS
 *************************************************************************/

// Print a log point to show that we're still awake.
static void awake()
{
    gAwakeCount++;
    // Note: must be LOG and not LOGX as LOGX uses a mutex
    // and this is an interrupt
    // Uncomment the following line to get the log point
    // (disabled by default to save bandwidth)
    //AQ_NRG_LOG(EVENT_AWAKE, gAwakeCount);
}

// Update the current time.
static void updateTime(time_t timeUTC)
{
    time_t diff;

    MTX_LOCK(gMtx);

    statisticsTimeUpdate(timeUTC);
    diff = timeUTC - time(NULL);

    // Update the stored last measurement times to
    // take account of the change
    gLastMeasurementTimeBme280Seconds += diff;
    gLastMeasurementTimeLis3dhSeconds += diff;
    gLastMeasurementTimeSi7210Seconds += diff;
    gLastMeasurementTimeSi1133Seconds += diff;
    gLastSleepTimeModemSeconds += diff;

    set_time(timeUTC);
    gTimeUpdate = timeUTC;
    AQ_NRG_LOGX(EVENT_TIME_SET, timeUTC);

    MTX_UNLOCK(gMtx);
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

// Work out the amount of system active energy
// that is yet to be allocated to an action.
static unsigned long long int activeEnergyUsedNWH()
{
    unsigned long long int energyNWH = 0;

    if (gpProcessTimer != NULL) {
        energyNWH = ((unsigned long long int) gpProcessTimer->read_ms()) * PROCESSOR_POWER_ACTIVE_NW / 3600000;
        energyNWH -= gSystemActiveEnergyAllocatedNWH;
        gSystemActiveEnergyAllocatedNWH = energyNWH;
    }

    return energyNWH;
}

// Check whether this thread has been terminated.
// Note: the signal is automagically reset after it has been received,
// hence it is necessary to pass in a pointer to the same "keepGoing" flag
// so that can be set in order to remember when the signal went south.
static bool threadContinue(bool *pKeepGoing)
{
    return (*pKeepGoing = *pKeepGoing && (Thread::signal_wait(TERMINATE_THREAD_SIGNAL, 0).status == osOK));
}

// Callback function to tell the modem to keep going (or not).
static bool modemKeepGoing(void *pKeepGoing) {
    return threadContinue((bool *) pKeepGoing);
}

// Do both sorts of reporting (get time as an option).
static void reporting(Action *pAction, bool *pKeepGoing, bool getTime)
{
    DataContents contents;
    Data *pData;
    bool cellularMeasurementTaken = false;
    time_t timeUTC;
    char imeiString[MODEM_IMEI_LENGTH];
    unsigned int bytesTransmitted = 0;
    int x;

    // Initialise the cellular modem
    if (modemInit(SIM_PIN, APN, USERNAME, PASSWORD) == ACTION_DRIVER_OK) {
        // Obtain the IMEI
        if (threadContinue(pKeepGoing)) {
            // Fill with something unique so that we can see
            // something has gone wrong
            memset (imeiString, '6', sizeof(imeiString));
            imeiString[sizeof(imeiString) - 1] = 0;
            if ((modemGetImei(imeiString) == ACTION_DRIVER_OK)) {
                // Calling our own atoi() here rather than clib's atoi()
                // as that leads to memory space issues with a multithreading
                // configuration of RTX ("increase OS_THREAD_LIBSPACE_NUM"
                // it said; you can guess what I said...)
                AQ_NRG_LOGX(EVENT_IMEI_ENDING, asciiToInt(&(imeiString[9])));
            } else {
                // Carry on anyway, better to make a report with
                // an un-initialised IMEI
                AQ_NRG_LOGX(EVENT_GET_IMEI_FAILURE, 0);
            }
        }

        // Add the current statistics
        statisticsGet(&contents.statistics);
        bytesTransmitted = contents.statistics.cellularBytesTransmittedSinceReset;
        if (pDataAlloc(NULL, DATA_TYPE_STATISTICS, 0, &contents) == NULL) {
            AQ_NRG_LOGX(EVENT_DATA_ITEM_ALLOC_FAILURE, DATA_TYPE_STATISTICS);
            AQ_NRG_LOGX(EVENT_DATA_CURRENT_SIZE_BYTES, dataGetBytesUsed());
        }

        // Collect the stored log entries
        // Note: since we have to commit to removing the log items from the store
        // on calling getLog() this is the one case where we preallocate a data
        // item, to make sure when is available, and then copy the contents in later
        dataLockList();
        while ((getNumLogEntries() > 0) &&
               ((pData = pDataAlloc(NULL, DATA_TYPE_LOG, 0, NULL)) != NULL)) {
            contents.log.numItems = getLog(contents.log.log, ARRAY_SIZE(contents.log.log));
            contents.log.index = gLogIndex;
            gLogIndex++;
            contents.log.logClientVersion = LOG_VERSION;
            contents.log.logApplicationVersion = APPLICATION_LOG_VERSION;
            memcpy(&(pData->contents), &contents, gDataSizeOfContents[DATA_TYPE_LOG]);
        }
        dataUnlockList();

        // Make a connection
        if (threadContinue(pKeepGoing)) {
            if (modemConnect(modemKeepGoing, pKeepGoing) == ACTION_DRIVER_OK) {
                // Get the time if required
                if (threadContinue(pKeepGoing) && getTime) {
                    if (modemGetTime(&timeUTC) == ACTION_DRIVER_OK) {
                        updateTime(timeUTC);
                    } else {
                        AQ_NRG_LOGX(EVENT_GET_TIME_FAILURE, 0);
                    }
                }
                // Get the cellular measurements
                if (threadContinue(pKeepGoing)) {
                    memset(&contents, 0, sizeof(contents));
                    cellularMeasurementTaken |= (getCellularSignalRx(&contents.cellular.rsrpDbm,
                                                                     &contents.cellular.rssiDbm,
                                                                     &contents.cellular.rsrqDb,
                                                                     &contents.cellular.snrDb) == ACTION_DRIVER_OK);
                    cellularMeasurementTaken |= (getCellularSignalTx(&contents.cellular.transmitPowerDbm) == ACTION_DRIVER_OK);
                    cellularMeasurementTaken |= (getCellularChannel(&contents.cellular.cellId,
                                                                    &contents.cellular.earfcn,
                                                                    &contents.cellular.ecl) == ACTION_DRIVER_OK);
                    if (cellularMeasurementTaken) {
                        // The cost of the last modem activity is used here
                        // since we won't know the total until after the
                        // transmission has completed
                        pAction->energyCostNWH = gLastModemEnergyNWH;
                        if (pDataAlloc(pAction, DATA_TYPE_CELLULAR, 0, &contents) == NULL) {
                            AQ_NRG_LOGX(EVENT_DATA_ITEM_ALLOC_FAILURE, DATA_TYPE_CELLULAR);
                            AQ_NRG_LOGX(EVENT_DATA_CURRENT_SIZE_BYTES, dataGetBytesUsed());
                        }
                    }
                }
                // Send reports
                if (threadContinue(pKeepGoing)) {
                    x = modemSendReports(IOT_SERVER_IP_ADDRESS, IOT_SERVER_PORT,
                                         imeiString, modemKeepGoing, pKeepGoing);
                    if (x == ACTION_DRIVER_OK) {
                        actionCompleted(pAction);
                    } else {
                        actionTriedAndFailed(pAction);
                        AQ_NRG_LOGX(EVENT_SEND_FAILURE, x);
                    }
                }
            } else {
                actionTriedAndFailed(pAction);
                AQ_NRG_LOGX(EVENT_CONNECT_FAILURE, modemGetLastConnectErrorCode());
            }
        }
    } else {
        actionTriedAndFailed(pAction);
        AQ_NRG_LOGX(EVENT_ACTION_DRIVER_INIT_FAILURE, ACTION_TYPE_REPORT);
    }

    if (pAction->state == ACTION_STATE_TRIED_AND_FAILED) {
        gReportNumFailures++;
    }

    // Work out the cost of doing all that so that we
    // can report it next time
    statisticsGet(&contents.statistics);
    bytesTransmitted = contents.statistics.cellularBytesTransmittedSinceReset - bytesTransmitted;
    MTX_LOCK(gMtx);
    timeUTC = 0;  // Just re-using this variable since it happens to be lying around
    if (!gModemOff) {
       timeUTC = time(NULL) - gLastSleepTimeModemSeconds;
    }
    gLastModemEnergyNWH = modemEnergyNWH(timeUTC, bytesTransmitted) +
                          gSystemIdleEnergyPropNWH + activeEnergyUsedNWH();
    gLastSleepTimeModemSeconds = time(NULL);
    // Write this into the action, even though it won't be reported,
    // so that the correct value is used in the energy calculation
    // next time we wake up
    pAction->energyCostNWH = gLastModemEnergyNWH;
    MTX_UNLOCK(gMtx);

    if (modemIsR4() || (modemIsN2() && CELLULAR_N211_OFF_WHEN_NOT_IN_USE)) {
        // Shut the modem down again
        modemDeinit();
        gModemOff = true;
        AQ_NRG_LOGX(EVENT_CELLULAR_OFF_NOW, 0);
    } else {
        gModemOff = false;
        // if we've failed too many times, let the modem
        // have a rest
        if (gReportNumFailures >= MAX_NUM_REPORT_FAILURES) {
            gReportNumFailures = 0;
            modemDeinit();
            gModemOff = true;
            AQ_NRG_LOGX(EVENT_CELLULAR_OFF_NOW, 0);
        }
    }

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
            if (threadContinue(pKeepGoing)) {
                if (getHumidity(&contents.humidity.percentage) == ACTION_DRIVER_OK) {
                    actionCompleted(pAction);
                    // The environment sensor is on all the time so the energy
                    // consumed is the power consumed since the last measurement
                    // times the time, plus any individual cost associated with
                    // taking this reading.
                    MTX_LOCK(gMtx);
                    pAction->energyCostNWH = (((unsigned long long int) (time(NULL) - gLastMeasurementTimeBme280Seconds)) *
                                              BME280_POWER_IDLE_NW / 3600) + BME280_ENERGY_READING_NWH +
                                             gSystemIdleEnergyPropNWH + activeEnergyUsedNWH();
                    gLastMeasurementTimeBme280Seconds = time(NULL);
                    MTX_UNLOCK(gMtx);
                    if (pDataAlloc(pAction, DATA_TYPE_HUMIDITY, 0, &contents) == NULL) {
                        AQ_NRG_LOGX(EVENT_DATA_ITEM_ALLOC_FAILURE, DATA_TYPE_HUMIDITY);
                        AQ_NRG_LOGX(EVENT_DATA_CURRENT_SIZE_BYTES, dataGetBytesUsed());
                    }
                } else {
                    actionTriedAndFailed(pAction);
                }
            }
        } else {
            AQ_NRG_LOGX(EVENT_ACTION_DRIVER_INIT_FAILURE, ACTION_TYPE_MEASURE_HUMIDITY);
        }
    } else {
        AQ_NRG_LOGX(EVENT_ACTION_DRIVER_HEAP_TOO_LOW, pAction->type);
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
            if (threadContinue(pKeepGoing)) {
                if (getPressure(&contents.atmosphericPressure.pascalX100) == ACTION_DRIVER_OK) {
                    actionCompleted(pAction);
                    // The environment sensor is on all the time so the energy
                    // consumed is the power consumed since the last measurement
                    // times the time, plus any individual cost associated with
                    // taking this reading.
                    MTX_LOCK(gMtx);
                    pAction->energyCostNWH = (((unsigned long long int) (time(NULL) - gLastMeasurementTimeBme280Seconds)) *
                                              BME280_POWER_IDLE_NW / 3600) + BME280_ENERGY_READING_NWH +
                                             gSystemIdleEnergyPropNWH + activeEnergyUsedNWH();
                    gLastMeasurementTimeBme280Seconds = time(NULL);
                    MTX_UNLOCK(gMtx);
                    if (pDataAlloc(pAction, DATA_TYPE_ATMOSPHERIC_PRESSURE, 0, &contents) == NULL) {
                        AQ_NRG_LOGX(EVENT_DATA_ITEM_ALLOC_FAILURE, DATA_TYPE_ATMOSPHERIC_PRESSURE);
                        AQ_NRG_LOGX(EVENT_DATA_CURRENT_SIZE_BYTES, dataGetBytesUsed());
                    }
                } else {
                    actionTriedAndFailed(pAction);
                }
            }
        } else {
            AQ_NRG_LOGX(EVENT_ACTION_DRIVER_INIT_FAILURE, ACTION_TYPE_MEASURE_ATMOSPHERIC_PRESSURE);
        }
    } else {
        AQ_NRG_LOGX(EVENT_ACTION_DRIVER_HEAP_TOO_LOW, pAction->type);
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
            if (threadContinue(pKeepGoing)) {
                if (getTemperature(&contents.temperature.cX100) == ACTION_DRIVER_OK) {
                    actionCompleted(pAction);
                    // The environment sensor is on all the time so the energy
                    // consumed is the power consumed since the last measurement
                    // times the time, plus any individual cost associated with
                    // taking this reading.
                    MTX_LOCK(gMtx);
                    pAction->energyCostNWH = (((unsigned long long int) (time(NULL) - gLastMeasurementTimeBme280Seconds)) *
                                              BME280_POWER_IDLE_NW / 3600) + BME280_ENERGY_READING_NWH +
                                             gSystemIdleEnergyPropNWH + activeEnergyUsedNWH();
                    gLastMeasurementTimeBme280Seconds = time(NULL);
                    MTX_UNLOCK(gMtx);
                    if (pDataAlloc(pAction, DATA_TYPE_TEMPERATURE, 0, &contents) == NULL) {
                        AQ_NRG_LOGX(EVENT_DATA_ITEM_ALLOC_FAILURE, DATA_TYPE_TEMPERATURE);
                        AQ_NRG_LOGX(EVENT_DATA_CURRENT_SIZE_BYTES, dataGetBytesUsed());
                    }
                } else {
                    actionTriedAndFailed(pAction);
                }
            }
        } else {
            AQ_NRG_LOGX(EVENT_ACTION_DRIVER_INIT_FAILURE, ACTION_TYPE_MEASURE_TEMPERATURE);
        }
    } else {
        AQ_NRG_LOGX(EVENT_ACTION_DRIVER_HEAP_TOO_LOW, pAction->type);
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
            if (threadContinue(pKeepGoing)) {
                if (getLight(&contents.light.lux, &contents.light.uvIndexX1000) == ACTION_DRIVER_OK) {
                    actionCompleted(pAction);
                    // The light sensor is on all the time so the energy
                    // consumed is the power consumed since the last measurement
                    // times the time, plus any individual cost associated with
                    // taking this reading.
                    MTX_LOCK(gMtx);
                    pAction->energyCostNWH = (((unsigned long long int) (time(NULL) - gLastMeasurementTimeSi1133Seconds)) *
                                              SI1133_POWER_IDLE_NW / 3600) + SI1133_ENERGY_READING_NWH +
                                             gSystemIdleEnergyPropNWH + activeEnergyUsedNWH();
                    gLastMeasurementTimeSi1133Seconds = time(NULL);
                    MTX_UNLOCK(gMtx);
                    if (pDataAlloc(pAction, DATA_TYPE_LIGHT, 0, &contents) == NULL) {
                        AQ_NRG_LOGX(EVENT_DATA_ITEM_ALLOC_FAILURE, DATA_TYPE_LIGHT);
                        AQ_NRG_LOGX(EVENT_DATA_CURRENT_SIZE_BYTES, dataGetBytesUsed());
                    }
                } else {
                    actionTriedAndFailed(pAction);
                }
            }
        } else {
            actionTriedAndFailed(pAction);
            AQ_NRG_LOGX(EVENT_ACTION_DRIVER_INIT_FAILURE, ACTION_TYPE_MEASURE_LIGHT);
        }

        // Shut the device down again
        si1133Deinit();
    } else {
        AQ_NRG_LOGX(EVENT_ACTION_DRIVER_HEAP_TOO_LOW, pAction->type);
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
        if (lis3dhInit(LIS3DH_DEFAULT_ADDRESS) == ACTION_DRIVER_OK) {
            if (getAcceleration(&contents.acceleration.xGX1000, &contents.acceleration.yGX1000,
                                &contents.acceleration.zGX1000) == ACTION_DRIVER_OK) {
                actionCompleted(pAction);
                // The accelerometer is on all the time so the energy
                // consumed is the power consumed since the last measurement
                // times the time, plus any individual cost associated with
                // taking this reading.
                MTX_LOCK(gMtx);
                pAction->energyCostNWH = (((unsigned long long int) (time(NULL) - gLastMeasurementTimeLis3dhSeconds)) *
                                          LIS3DH_POWER_IDLE_NW / 3600) + LIS3DH_ENERGY_READING_NWH +
                                         gSystemIdleEnergyPropNWH + activeEnergyUsedNWH();
                gLastMeasurementTimeLis3dhSeconds = time(NULL);
                MTX_UNLOCK(gMtx);
            if (pDataAlloc(pAction, DATA_TYPE_ACCELERATION, 0, &contents) == NULL) {
                    AQ_NRG_LOGX(EVENT_DATA_ITEM_ALLOC_FAILURE, DATA_TYPE_ACCELERATION);
                    AQ_NRG_LOGX(EVENT_DATA_CURRENT_SIZE_BYTES, dataGetBytesUsed());
                }
            } else {
                actionTriedAndFailed(pAction);
            }
        } else {
            actionTriedAndFailed(pAction);
            AQ_NRG_LOGX(EVENT_ACTION_DRIVER_INIT_FAILURE, ACTION_TYPE_MEASURE_ACCELERATION);
       }
    } else {
        AQ_NRG_LOGX(EVENT_ACTION_DRIVER_HEAP_TOO_LOW, pAction->type);
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

    AQ_NRG_LOGX(EVENT_POSITION_BACK_OFF_SECONDS, gPositionFixSkipsRequired * WAKEUP_INTERVAL_SECONDS);
    if (gPositionNumFixesSkipped >= gPositionFixSkipsRequired) {
        if (heapIsAboveMargin(MODEM_HEAP_REQUIRED_BYTES)) {
            // Initialise the GNSS device and wait for a measurement
            // to pop-out.
            if (zoem8Init(ZOEM8_DEFAULT_ADDRESS) == ACTION_DRIVER_OK) {
                timer.reset();
                timer.start();
                statisticsIncPositionAttempts();
#if defined(__CC_ARM) || (defined(__ARMCC_VERSION) && (__ARMCC_VERSION >= 6010050))
#pragma diag_suppress 1293  //  suppressing warning "assignment in condition" on ARMCC
#endif
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

                // GNSS is only switched on when required and so
                // the energy cost is the time spent achieving a fix.
                MTX_LOCK(gMtx);
                pAction->energyCostNWH = (((unsigned long long int) (timer.read_ms() / 1000)) * ZOEM8_POWER_ACTIVE_NW / 3600) +
                                         gSystemIdleEnergyPropNWH + activeEnergyUsedNWH();
                MTX_UNLOCK(gMtx);

                if (gotFix) {
                    // Reset the number of skips required and
                    // the skip count as we can get position now
                    gPositionFixSkipsRequired = 0;
                    gPositionNumFixesSkipped = 0;
                    gPositionNumFixesFailedNoBackOff = 0;
                    // Update stats and complete the action
                    statisticsIncPositionSuccess();
                    statisticsLastSVs(SVs);
                    actionCompleted(pAction);
                    // Since GNSS is able to get a fix, get the time also
                    if (getTime(&timeUTC) == ACTION_DRIVER_OK) {
                        updateTime(timeUTC);
                    }
                    if (pDataAlloc(pAction, DATA_TYPE_POSITION, 0, &contents) == NULL) {
                        AQ_NRG_LOGX(EVENT_DATA_ITEM_ALLOC_FAILURE, DATA_TYPE_POSITION);
                        AQ_NRG_LOGX(EVENT_DATA_CURRENT_SIZE_BYTES, dataGetBytesUsed());
                    }
                    // Since GNSS is able to get a fix, get the time also
                    if (getTime(&timeUTC) == ACTION_DRIVER_OK) {
                        updateTime(timeUTC);
                    }
                } else {
                    // Didn't achieve a fix, decide what to do
                    if (gPositionFixSkipsRequired == 0) {
                        gPositionNumFixesFailedNoBackOff++;
                        // If we've tried without success for the no back-off
                        // period, start skipping
                        if (gPositionNumFixesFailedNoBackOff * WAKEUP_INTERVAL_SECONDS > LOCATION_FIX_NO_BACK_OFF_SECONDS) {
                            gPositionFixSkipsRequired = 1;
                            gPositionNumFixesSkipped = 0;
                        }
                    } else {
                        // We've been skipping already, double the number
                        // of skips up to the limit
                        if (gPositionFixSkipsRequired * WAKEUP_INTERVAL_SECONDS * 2 < LOCATION_FIX_MAX_PERIOD_SECONDS) {
                            gPositionFixSkipsRequired *= 2;
                        } else {
                            gPositionFixSkipsRequired = LOCATION_FIX_MAX_PERIOD_SECONDS / WAKEUP_INTERVAL_SECONDS;
                        }
                        gPositionNumFixesSkipped = 0;
                    }

                    actionTriedAndFailed(pAction);
                }
            } else {
                // Keep track of the number of position fix attempts skipped
                gPositionNumFixesSkipped++;
                AQ_NRG_LOGX(EVENT_ACTION_DRIVER_INIT_FAILURE, ACTION_TYPE_MEASURE_POSITION);
            }
            // Shut the device down again
            zoem8Deinit();
        } else {
            // Keep track of the number of position fix attempts skipped
            gPositionNumFixesSkipped++;
            AQ_NRG_LOGX(EVENT_ACTION_DRIVER_HEAP_TOO_LOW, pAction->type);
        }
    } else {
        // Keep track of the number of position fix attempts skipped
        gPositionNumFixesSkipped++;
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
        if (si7210Init(SI7210_DEFAULT_ADDRESS) == ACTION_DRIVER_OK) {
            if (getFieldStrength(&contents.magnetic.teslaX1000) == ACTION_DRIVER_OK) {
                actionCompleted(pAction);
                // The hall effect sensor is on all the time so the energy
                // consumed is the power consumed since the last measurement
                // times the time, plus any individual cost associated with
                // taking this reading.
                MTX_LOCK(gMtx);
                pAction->energyCostNWH = (((unsigned long long int) (time(NULL) - gLastMeasurementTimeSi7210Seconds)) *
                                          SI7210_POWER_IDLE_NW / 3600) + SI7210_ENERGY_READING_NWH +
                                         gSystemIdleEnergyPropNWH + activeEnergyUsedNWH();
                gLastMeasurementTimeSi7210Seconds = time(NULL);
                MTX_UNLOCK(gMtx);
                if (pDataAlloc(pAction, DATA_TYPE_MAGNETIC, 0, &contents) == NULL) {
                    AQ_NRG_LOGX(EVENT_DATA_ITEM_ALLOC_FAILURE, DATA_TYPE_MAGNETIC);
                    AQ_NRG_LOGX(EVENT_DATA_CURRENT_SIZE_BYTES, dataGetBytesUsed());
                }
            } else {
                actionTriedAndFailed(pAction);
            }
        } else {
            actionTriedAndFailed(pAction);
            AQ_NRG_LOGX(EVENT_ACTION_DRIVER_INIT_FAILURE, ACTION_TYPE_MEASURE_MAGNETIC);
       }
    } else {
        AQ_NRG_LOGX(EVENT_ACTION_DRIVER_HEAP_TOO_LOW, pAction->type);
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

                MTX_LOCK(gMtx);
                if (gpBleTimer != NULL) {
                    pAction->energyCostNWH = gpBleTimer->read_ms() * BLE_POWER_ACTIVE_NW / 3600000;
                    pAction->energyCostNWH -= gBleActiveEnergyAllocatedNWH;
                    gBleActiveEnergyAllocatedNWH = pAction->energyCostNWH;
                }
                pAction->energyCostNWH += (((unsigned long long int) (time(NULL) - gLastMeasurementTimeBleSeconds)) *
                                           BLE_POWER_IDLE_NW / 3600) + gSystemIdleEnergyPropNWH +
                                          activeEnergyUsedNWH();
                gLastMeasurementTimeBleSeconds = time(NULL);
                MTX_UNLOCK(gMtx);

                if (pDataAlloc(pAction, DATA_TYPE_BLE, 0, &contents) == NULL) {
                    AQ_NRG_LOGX(EVENT_DATA_ITEM_ALLOC_FAILURE, DATA_TYPE_BLE);
                    AQ_NRG_LOGX(EVENT_DATA_CURRENT_SIZE_BYTES, dataGetBytesUsed());
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
    int eventQueueId;

    MBED_ASSERT(pAction->type == ACTION_TYPE_MEASURE_BLE);
    MBED_ASSERT(gpEventQueue != NULL);

#if !MBED_CONF_APP_DISABLE_PERIPHERAL_HW && !defined (TARGET_UBLOX_C030_U201)
    gpBleTimer = new Timer();
    gBleActiveEnergyAllocatedNWH = 0;
    if (heapIsAboveMargin(MODEM_HEAP_REQUIRED_BYTES)) {
        bleInit(BLE_PEER_DEVICE_NAME_PREFIX, GattCharacteristic::UUID_BATTERY_LEVEL_STATE_CHAR, BLE_PEER_NUM_DATA_ITEMS, gpEventQueue, false);
        eventQueueId = gpEventQueue->call_every(PROCESSOR_IDLE_MS, callback(checkBleProgress, pAction));
        bleRun(BLE_ACTIVE_TIME_MS);
        gpBleTimer->reset();
        gpBleTimer->start();
        while (threadContinue(pKeepGoing) && (gpBleTimer->read_ms() < BLE_ACTIVE_TIME_MS)) {
            Thread::wait(PROCESSOR_IDLE_MS);
        }
        gpBleTimer->stop();

        actionCompleted(pAction);
        gpEventQueue->cancel(eventQueueId);
        bleDeinit();
    } else {
        AQ_NRG_LOGX(EVENT_ACTION_DRIVER_HEAP_TOO_LOW, pAction->type);
    }
    delete gpBleTimer;
    gpBleTimer = NULL;
#endif
    // Done with this task now
    *pKeepGoing = false;
}

// The callback that forms an action thread
static void doAction(Action *pAction)
{
    bool keepGoing = true;

    AQ_NRG_LOGX(EVENT_ACTION_THREAD_STARTED, pAction->type);
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

    // If the action has not run, mark it as aborted
    if (!hasActionRun(pAction)) {
        actionAborted(pAction);
    }

    // Whether successful or not, add any energy used to
    // the statistics
    statisticsAddEnergy(pAction->energyCostNWH);

    AQ_NRG_LOGX(EVENT_ACTION_THREAD_TERMINATED, pAction->type);
    AQ_NRG_LOGX(EVENT_THIS_STACK_MIN_LEFT, osThreadGetStackSize(Thread::gettid()) - osThreadGetStackSpace(Thread::gettid()));
    if (pAction->energyCostNWH < 0xFFFFFFFF) {
        AQ_NRG_LOGX(EVENT_ENERGY_USED_NWH, (unsigned int) pAction->energyCostNWH);
    } else {
        AQ_NRG_LOGX(EVENT_ENERGY_USED_UWH, (unsigned int) (pAction->energyCostNWH / 1000));
    }
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
            AQ_NRG_LOGX(EVENT_ACTION_THREAD_SIGNALLED, 0);
        }
    }

    // Wait for them all to end
    while ((x = checkThreadsRunning()) > 0) {
        Thread::wait(PROCESSOR_IDLE_MS);
        AQ_NRG_LOGX(EVENT_ACTION_THREADS_RUNNING, x);
    }

    AQ_NRG_LOGX(EVENT_ALL_THREADS_TERMINATED, 0);
}

// Determine the actions to perform
static ActionType processorActionList()
{
    ActionType actionType;
    ActionType actionOverTheBrink;
    unsigned long long int energyRequiredNWH = 0;
    unsigned long long int energyRequiredTotalNWH = 0;
    unsigned long long int energyAvailableNWH = getEnergyAvailableNWH();

    if (energyAvailableNWH < 0xFFFFFFFF) {
        AQ_NRG_LOGX(EVENT_ENERGY_AVAILABLE_NWH, (unsigned int) energyAvailableNWH);
    } else {
        AQ_NRG_LOGX(EVENT_ENERGY_AVAILABLE_UWH, (unsigned int) (energyAvailableNWH / 1000));
    }

    // Rank the action list
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

#if !ENABLE_LOCATION
    actionType = actionRankDelType(ACTION_TYPE_MEASURE_POSITION);
#endif

    // Go through the list and remove any items where we already have
    // enough data items sitting in the queue
    while (actionType != ACTION_TYPE_NULL) {
        if (dataCountType(gDataType[actionType]) > PROCESSOR_MAX_NUM_DATA_TYPE) {
            AQ_NRG_LOGX(EVENT_ACTION_REMOVED_QUEUE_LIMIT, actionType);
            actionType = actionRankDelType(actionType);
        } else {
            actionType = actionRankNextType();
        }
    }

    // Now go through the list again and calculate how much energy is
    // required, dropping any items that take us over the edge
    actionType = actionRankFirstType();
    while (actionType != ACTION_TYPE_NULL) {
        energyRequiredTotalNWH = 0;
        actionOverTheBrink = ACTION_TYPE_NULL;
        while ((actionOverTheBrink == ACTION_TYPE_NULL) &&
               (actionType != ACTION_TYPE_NULL)) {
            energyRequiredNWH = actionEnergyNWH(actionType);
            energyRequiredTotalNWH += energyRequiredNWH;
            if (energyRequiredTotalNWH > energyAvailableNWH) {
                actionOverTheBrink = actionType;
            }
            actionType = actionRankNextType();
        }
        if (actionOverTheBrink != ACTION_TYPE_NULL) {
            AQ_NRG_LOGX(EVENT_ACTION_REMOVED_ENERGY_LIMIT, actionOverTheBrink);
            if (energyRequiredNWH < 0xFFFFFFFF) {
                AQ_NRG_LOGX(EVENT_ENERGY_REQUIRED_NWH, (unsigned int) energyRequiredNWH);
            } else {
                AQ_NRG_LOGX(EVENT_ENERGY_REQUIRED_UWH, (unsigned int) (energyRequiredNWH / 1000));
            }
            // Increment the skip count if this was GNSS
            if (actionOverTheBrink == ACTION_TYPE_MEASURE_POSITION) {
                gPositionNumFixesSkipped++;
            }
            actionType = actionRankDelType(actionOverTheBrink);
        }
    }
    if (energyRequiredNWH < 0xFFFFFFFF) {
        AQ_NRG_LOGX(EVENT_ENERGY_REQUIRED_TOTAL_NWH, (unsigned int) energyRequiredTotalNWH);
    } else {
        AQ_NRG_LOGX(EVENT_ENERGY_REQUIRED_TOTAL_UWH, (unsigned int) (energyRequiredTotalNWH / 1000));
    }

    return actionRankFirstType();
}

// Determine the wake-up reason.
static WakeUpReason processorWakeUpReason()
{
    WakeUpReason wakeUpReason = WAKE_UP_RTC;
    DataContents contents;

    if (gJustBooted) {
        wakeUpReason = WAKE_UP_POWER_ON;
#if TARGET_UBLOX_EVK_NINA_B1
        // From section 18.8.3 of the NRF52832 product spec
        if (NRF_POWER->RESETREAS & 0x04) {
            wakeUpReason = WAKE_UP_SOFT_RESET;
        }
        // Not sure why but when testing the watchdog
        // I found that the system did reset but that
        // this watchdog bit was never set
        if (NRF_POWER->RESETREAS & 0x02) {
            wakeUpReason = WAKE_UP_WATCHDOG;
        }
        // Do this last as there's an errata item indicating
        // that other bits may also be set when this one
        // is and in this case those bits should be ignored
        if (NRF_POWER->RESETREAS & 0x01) {
            wakeUpReason = WAKE_UP_PIN_RESET;
        }
        NRF_POWER->RESETREAS = 0;
#endif
        gJustBooted = false;
    } else {
        if (getFieldStrengthInterruptFlag()) {
            wakeUpReason = WAKE_UP_MAGNETIC;
            clearFieldStrengthInterruptFlag();
        }
        if (getAccelerationInterruptFlag()) {
           wakeUpReason = WAKE_UP_ACCELERATION;
           clearAccelerationInterruptFlag();
           // If we woke up due to the accelerometer
           // we must be moving so reset any back-off
           // on GNSS fix attempts
           gPositionFixSkipsRequired = 0;
           gPositionNumFixesSkipped = 0;
           gPositionNumFixesFailedNoBackOff = 0;
       }
    }

    contents.wakeUpReason.reason = wakeUpReason;
    if (pDataAlloc(NULL, DATA_TYPE_WAKE_UP_REASON, 0, &contents) == NULL) {
        AQ_NRG_LOGX(EVENT_DATA_ITEM_ALLOC_FAILURE, DATA_TYPE_WAKE_UP_REASON);
        AQ_NRG_LOGX(EVENT_DATA_CURRENT_SIZE_BYTES, dataGetBytesUsed());
    }

    return wakeUpReason;
}

#ifndef DISABLE_ENERGY_CHOOSER
// Set the energy source.
static void processorSetEnergySource(unsigned char energySource)
{
    DataContents contents;

    // Enable the given energy source
    setEnergySource(energySource);
    AQ_NRG_LOGX(EVENT_ENERGY_SOURCE_SET, getEnergySource());
    // Set the chosen energy source, which must be free'd
    // by now with a call to processorAgeEnergySource()
    CHOOSE_ENERGY_SOURCE(energySource);

    contents.energySource.x = energySource;
    if (pDataAlloc(NULL, DATA_TYPE_ENERGY_SOURCE, 0, &contents) == NULL) {
        AQ_NRG_LOGX(EVENT_DATA_ITEM_ALLOC_FAILURE, DATA_TYPE_ENERGY_SOURCE);
        AQ_NRG_LOGX(EVENT_DATA_CURRENT_SIZE_BYTES, dataGetBytesUsed());
    }
}

// Select the best energy source from previous choices.
static unsigned char processorBestRecentEnergySource()
{
    unsigned char energySource = ENERGY_SOURCE_DEFAULT;
    unsigned int y = ARRAY_SIZE(gEnergyChoice);
    unsigned int count[ENERGY_SOURCES_MAX_NUM];

    memset(count, 0, sizeof(count));

    if (y > gNumWakeups) {
        y = gNumWakeups;
    }

    for (unsigned int x = 0; x < y; x++) {
        // If the lower nibble is non-zero then the energy
        // source specified in the upper nibble as a good one
        if ((GET_ENERGY_SOURCE_GOOD(gEnergyChoice[x])) &&
            (GET_ENERGY_SOURCE(gEnergyChoice[x]) > 0) &&
            (GET_ENERGY_SOURCE(gEnergyChoice[x]) <= ENERGY_SOURCES_MAX_NUM)) {
            (count[GET_ENERGY_SOURCE(gEnergyChoice[x]) - 1])++;
        }
    }

    // Chose the highest scoring one
    for (unsigned int x = 0; x < ARRAY_SIZE(count); x++) {
        if (count[x] > count[energySource - 1]) {
            energySource = x + 1;
        }
    }

    return energySource;
}
#endif

// Age the energy source.
static void processorAgeEnergySource()
{
    // Shuffle the energy sources down in the list
    for (unsigned int x = 0; x < ARRAY_SIZE(gEnergyChoice) - 1; x++) {
        gEnergyChoice[x + 1] = gEnergyChoice[x];
    }
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

        gLogSuspendTime = 0;
        gLogIndex = 0;
        gTimeUpdate = 0;
        gLastMeasurementTimeBme280Seconds = 0;
        gLastMeasurementTimeLis3dhSeconds = 0;
        gLastMeasurementTimeSi7210Seconds = 0;
        gLastMeasurementTimeSi1133Seconds = 0;
        gLastSleepTimeModemSeconds = 0;
        gLastMeasurementTimeBleSeconds = 0;
        gLastModemEnergyNWH = 0;
        memset(gVIn, 0, sizeof(gVIn));
        gVInCount = 0;
        gNumWakeups = 0;
        gNumEnergeticWakeups= 0;
        gPositionFixSkipsRequired = 0;
        gPositionNumFixesSkipped = 0;
        gPositionNumFixesFailedNoBackOff = 0;
        gReportNumFailures = 0;
        gModemOff = true;

        CHOOSE_ENERGY_SOURCE(ENERGY_SOURCE_DEFAULT);
    }

    // Useful to know when searching for memory leaks
    AQ_NRG_LOGX(EVENT_HEAP_LEFT, debugGetHeapLeft());
    AQ_NRG_LOGX(EVENT_STACK_MIN_LEFT, debugGetStackMinLeft());

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
    Ticker ticker;
    bool keepGoing = true;
    int vIn = 0;
    unsigned int vInCount = 0;
    unsigned char energySource = ENERGY_SOURCE_DEFAULT;
    time_t maxRunTime = MAX_RUN_TIME_SECONDS;
    WakeUpReason wakeUpReason;

    // This variable will be unused if logging is compiled out
    // so fix that here
    AQ_NRG_UNUSED(wakeUpReason);

    // gpEventQueue is used here as a flag to see if
    // we're already running: if we are, don't run again
    if (gpEventQueue == NULL) {
        gpEventQueue = pEventQueue;

        if (gNumWakeups == 0) {
            maxRunTime = MAX_RUN_FIRST_TIME_SECONDS;
        }

        gNumWakeups++;
        gAwakeCount = 0;
        ticker.attach(&awake, 1);
        gpProcessTimer = new Timer();
        gpProcessTimer->start();
        gSystemActiveEnergyAllocatedNWH = 0;

        // Feed the watchdog: doing this here has the effect
        // that if we end up running for too long (since
        // we only re-enter here if we aren't already running)
        // then the device will be reset
        feedWatchdog();
        resumeLog(((unsigned int) (time(NULL) - gLogSuspendTime)) * 1000000);

        wakeUpReason = processorWakeUpReason();
        AQ_NRG_LOGX(EVENT_WAKE_UP, wakeUpReason);
        AQ_NRG_LOGX(EVENT_CURRENT_TIME_UTC, time(NULL));

        AQ_NRG_LOGX(EVENT_V_BAT_OK_READING_MV, getVBatOkMV());
        AQ_NRG_LOGX(EVENT_V_PRIMARY_READING_MV, getVPrimaryMV());
        AQ_NRG_LOGX(EVENT_V_IN_READING_MV, getVInMV());
        AQ_NRG_LOGX(EVENT_ENERGY_SOURCE, getEnergySource());

        // If there is enough power to operate, perform some actions
        if (voltageIsBearable()) {
            gNumEnergeticWakeups++;
            debugPulseLed(20);
            AQ_NRG_LOGX(EVENT_PROCESSOR_RUNNING, voltageIsGood() + voltageIsNotBad() + voltageIsBearable());

            // Take a measurement of VIn
            vIn += getVInMV();
            vInCount++;

            statisticsWakeUp();

            // Derive the action to be performed
            actionType = processorActionList();
            AQ_NRG_LOGX(EVENT_ACTION, actionType);

            if (actionCount() > 0) {
                // Work out the proportion of the system idle energy to add to each
                // action's energy cost
                gSystemIdleEnergyPropNWH = ((unsigned long long int) (time(NULL) - gLogSuspendTime)) *
                                           PROCESSOR_POWER_IDLE_NW / 3600 / actionCount();
            }

            // Kick off actions while there's power and something to start
            while ((actionType != ACTION_TYPE_NULL) && voltageIsNotBad()) {
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
                                AQ_NRG_LOGX(EVENT_ACTION_THREAD_START_FAILURE, taskStatus);
                                delete gpActionThreadList[taskIndex];
                                gpActionThreadList[taskIndex] = NULL;
                            }
                            actionType = actionRankNextType();
                            AQ_NRG_LOGX(EVENT_ACTION, actionType);
                        } else {
                            AQ_NRG_LOGX(EVENT_ACTION_THREAD_ALLOC_FAILURE, 0);
                            Thread::wait(PROCESSOR_IDLE_MS); // Out of memory, need something to finish
                        }
                        AQ_NRG_LOGX(EVENT_HEAP_LEFT, debugGetHeapLeft());
                        AQ_NRG_LOGX(EVENT_STACK_MIN_LEFT, debugGetStackMinLeft());
                    } else {
                        AQ_NRG_LOGX(EVENT_ACTION_ALLOC_FAILURE, 0);
                        Thread::wait(PROCESSOR_IDLE_MS); // Out of memory, need something to finish
                    }
                }

                taskIndex++;
                if (taskIndex >= ARRAY_SIZE(gpActionThreadList)) {
                    taskIndex = 0;
                    AQ_NRG_LOGX(EVENT_ACTION_THREADS_RUNNING, checkThreadsRunning());
                    Thread::wait(PROCESSOR_IDLE_MS); // Relax a little once we've set a batch off
                }

                // Check if any threads have ended
                checkThreadsRunning();
            }

            AQ_NRG_LOGX(EVENT_POWER, voltageIsNotBad() + voltageIsBearable());

            // If we've got here then either we've kicked off all the required actions or
            // power is no longer good.  While power is good and we've not run out of time
            // take measurements of each VIN and do a background check on the progress of
            // the remaining actions.  Also make sure we keep running until we've measured
            // the VIN of all energy sources
            while ((checkThreadsRunning() > 0) && keepGoing) {
                // Take another measurement of VIn
                vIn += getVInMV();
                vInCount++;
                // Check for VBAT_OK going bad
                if (!voltageIsNotBad()) {
                    AQ_NRG_LOGX(EVENT_POWER, voltageIsNotBad() + voltageIsBearable());
                    keepGoing = false;
                // Check run-time
                } else if (gpProcessTimer->read_ms() / 1000 > maxRunTime) {
                    AQ_NRG_LOGX(EVENT_MAX_PROCESSOR_RUN_TIME_REACHED, gpProcessTimer->read_ms() / 1000);
                    keepGoing = false;
                // Or just wait
                } else {
                    Thread::wait(PROCESSOR_IDLE_MS);
                }
            }

            // We've now either done everything or power has gone.  If there are threads
            // still running, terminate them.
            terminateAllThreads();

            // The temperature/humidity/pressure sensor is de-initialised
            // here; it is otherwise left up to save time when using it for
            // more than one measurement type
            bme280Deinit();

            // Shut down I2C
            i2cDeinit();

            // If we've got here without the voltage going
            // bad then mark the current energy source as good
            if (voltageIsNotBad()) {
                SET_CURRENT_ENERGY_SOURCE_GOOD;
            }

            // Work out what VIn has been on average
            // and add it to the stored list
            if (vInCount > 0) {
                vIn /= vInCount;
                gVIn[getEnergySource() - 1] = vIn;
                if (gVInCount < ARRAY_SIZE(gVIn)) {
                    gVInCount++;
                }
                AQ_NRG_LOGX(EVENT_ENERGY_SOURCE, getEnergySource());
                AQ_NRG_LOGX(EVENT_V_IN_READING_AVERAGED_MV, vIn);
            }

#ifndef DISABLE_ENERGY_CHOOSER
            if (vInCount > 0) {
                // Work out which energy source to use next
                if (gVInCount >= ARRAY_SIZE(gVIn)) {
                    // Start with the existing energy choice and
                    // see if any of the others are better
                    energySource = getEnergySource();
                    if (energySource == 0) {
                        energySource = ENERGY_SOURCE_DEFAULT;
                    }
                    for (unsigned int x = 0; x < ARRAY_SIZE(gVIn); x++) {
                        if (gVIn[x] > gVIn[energySource - 1]) {
                            energySource = x + 1;
                        }
                    }
                    // Every 10'th wake-up try a different one,
                    // otherwise we'll never find a better source
                    if (gNumEnergeticWakeups % 10 == 0) {
                        energySource += (rand() % (ENERGY_SOURCES_MAX_NUM - 1)) + 1;
                        energySource = ((energySource - 1) % ENERGY_SOURCES_MAX_NUM) + 1;
                        AQ_NRG_LOGX(EVENT_ENERGY_SOURCE_CHOICE_RANDOM, energySource);
                    } else {
                        AQ_NRG_LOGX(EVENT_ENERGY_SOURCE_CHOICE_MEASURED, energySource);
                    }
                } else {
                    // If we haven't tried all the energy sources yet,
                    // just try the next one
                    energySource = getEnergySource();
                    energySource++;
                    if (energySource > ENERGY_SOURCES_MAX_NUM) {
                        energySource = 1;
                    }
                    AQ_NRG_LOGX(EVENT_ENERGY_SOURCE_CHOICE_SEQUENCE, energySource);
                }
                // Set the energy source for the next period
                processorAgeEnergySource();
                processorSetEnergySource(energySource);
            } else {
                processorAgeEnergySource();
            }
#else
            processorAgeEnergySource();
#endif

            AQ_NRG_LOGX(EVENT_DATA_CURRENT_SIZE_BYTES, dataGetBytesUsed());
            AQ_NRG_LOGX(EVENT_DATA_CURRENT_QUEUE_BYTES, dataGetBytesQueued());
            AQ_NRG_LOGX(EVENT_PROCESSOR_FINISHED, gpProcessTimer->read_ms() / 1000);
            statisticsSleep();
        } else {
#ifndef DISABLE_ENERGY_CHOOSER
            // Not enough energy to run, select the most successful
            // source from the history of energy source choices
            energySource = processorBestRecentEnergySource();
            AQ_NRG_LOGX(EVENT_ENERGY_SOURCE_CHOICE_HISTORY, energySource);
            processorAgeEnergySource();
            processorSetEnergySource(energySource);
#else
            processorAgeEnergySource();
#endif
            AQ_NRG_LOGX(EVENT_NOT_ENOUGH_POWER_TO_RUN_PROCESSOR, 0);
        }

        gpProcessTimer->stop();
        delete gpProcessTimer;
        gpProcessTimer = NULL;

        // Useful to know when searching for memory leaks
        AQ_NRG_LOGX(EVENT_HEAP_LEFT, debugGetHeapLeft());
        AQ_NRG_LOGX(EVENT_STACK_MIN_LEFT, debugGetStackMinLeft());
        AQ_NRG_LOGX(EVENT_HEAP_MIN_LEFT, debugGetHeapMinLeft());

        ticker.detach();

        AQ_NRG_LOGX(EVENT_RETURN_TO_SLEEP, time(NULL));
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
