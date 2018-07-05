/*
 * Copyright (C) u-blox Melbourn Ltd
 * u-blox Melbourn Ltd, Melbourn, UK
 *
 * All rights reserved.
 *
 * This source file is the sole property of u-blox Melbourn Ltd.
 * Reproduction or utilisation of this source in whole or part is
 * forbidden without the written consent of u-blox Melbourn Ltd.
 */

#ifndef _EH_DATA_H_
#define _EH_DATA_H_

#include <time.h>
#include <log.h>
#include <eh_action.h>

/**************************************************************************
 * MANIFEST CONSTANTS
 *************************************************************************/

/** The maximum length of a BLE device name.
 */
#define DATA_MAX_LEN_BLE_DEVICE_NAME 12

/** A guard timer on the sorting algorithm.  This is set to a large
 * number in order to allow unit tests, in which all of RAM is filled-up
 * with data items, to complete.
 */
#define DATA_SORT_GUARD_TIMER_MS 90000

/**************************************************************************
 * TYPES
 *************************************************************************/

/** The types of data.  If you add a new item here, don't forget to:
 *
 *   - add a struct for it,
 *   - add that struct to the DataContents union,
 *   - add an entry for it in gSizeOfContents[],
 *   - update dataDifference() to handle it,
 *   - update codecEncodeData() in eh_codec.cpp to encode it.
 *   - update the unit tests to be aware of it.
 *
 * If you modify a data item here don't forget to:
 *
 *   - update dataDifference() to handle it,
 *   - update codecEncodeData() in eh_codec.cpp to encode it.
 *   - update the unit tests to be aware of it.
 *
 * Note: order is important, don't change this unless you also change
 * sizeOfContents[] (in eh_data.cpp) and dataName[] (in eh_codec.cpp) to match.
 */
typedef enum {
    DATA_TYPE_NULL,
    DATA_TYPE_CELLULAR,
    DATA_TYPE_HUMIDITY,
    DATA_TYPE_ATMOSPHERIC_PRESSURE,
    DATA_TYPE_TEMPERATURE,
    DATA_TYPE_LIGHT,
    DATA_TYPE_ACCELERATION,
    DATA_TYPE_POSITION,
    DATA_TYPE_MAGNETIC,
    DATA_TYPE_BLE,
    DATA_TYPE_WAKE_UP_REASON,
    DATA_TYPE_ENERGY_SOURCE,
    DATA_TYPE_STATISTICS,
    DATA_TYPE_LOG,
    MAX_NUM_DATA_TYPES
} DataType;

/** Data struct for cellular.
 */
typedef struct {
    int rsrpDbm; /**< Strength of the wanted signal in dBm.*/
    int rssiDbm; /**< Total received signal strength in dBm.*/
    int rsrqDb; /**< Received signal quality in dB; see 3GPP 36.214.*/
    int snrDb; /**< Signal to noise ratio in dB.*/
    int transmitPowerDbm; /**< Transmit power in dBm.*/
    unsigned int cellId; /**< Cell ID unique across the network.*/
    unsigned int earfcn; /**< The current EARFCN (radio channel).*/
    unsigned char ecl; /**< The current coverage class (0 = GSM, 1 = up to 10 dB better link than GSM, 2 = up to 20 dB better link than GSM).*/
} DataCellular;

/** Data struct for humidity.
 */
typedef struct {
    unsigned char percentage; /**< Humidity as a percentage.*/
} DataHumidity;

/** Data struct for atmospheric pressure.
 */
typedef struct {
    unsigned int pascalX100; /**< Pressure in hundredths of a Pascal.*/
} DataAtmosphericPressure;

/** Data struct for temperature.
 */
typedef struct {
    signed int cX100; /**< Temperature in 100ths of a degree Celsius.*/
} DataTemperature;

/** Data struct for light.
 */
typedef struct {
    int lux;  /**< Light level in lux.*/
    int uvIndexX1000;  /**< UV Index in 1000th of an index.*/
} DataLight;

/** Data struct for acceleration.
 */
typedef struct {
    int xGX1000; /**< X-axis acceleration in thousandths of a gravity.*/
    int yGX1000; /**< Y-axis acceleration in thousandths of a gravity.*/
    int zGX1000; /**< Z-axis acceleration in thousandths of a gravity.*/
} DataAcceleration;

/** Data struct for position.
 */
typedef struct {
    int latitudeX10e7; /**< Latitude in 10 millionths of a degree.*/
    int longitudeX10e7; /**< Longitude in 10 millionths of a degree.*/
    int radiusMetres; /**< Radius of the position fix in metres.*/
    int altitudeMetres; /**< Altitude in metres.*/
    unsigned char speedMPS; /**< Speed in metres per second.*/
} DataPosition;

/** Data struct for the hall effect sensor.
 */
typedef struct {
    unsigned int teslaX1000; /**< Field strength in thousandths of a Tesla.*/
} DataMagnetic;

/** Data struct for BLE.
 */
typedef struct {
    char name[DATA_MAX_LEN_BLE_DEVICE_NAME]; /**< The name of the BLE device.*/
    unsigned char batteryPercentage; /**< Battery level as a percentage.*/
} DataBle;

/** The wake-up reasons.
 * Note: if you modify this then also modify
 * gpWakeUpReason in eh_codec.cpp.
 */
typedef enum {
    WAKE_UP_RTC,
    WAKE_UP_ACCELERATION,
    WAKE_UP_MAGNETIC,
    MAX_NUM_WAKE_UP_REASONS
} WakeUpReason;

/** Data struct for wake-up reason.
 */
typedef struct {
    WakeUpReason wakeUpReason; /**< The wake-up reason.*/
} DataWakeUpReason;

/** Data struct for energy source.
 */
typedef struct {
    unsigned char x; /**< The number of the chose energy souce.*/
} DataEnergySource;

/** Data struct for statistics.
 */
typedef struct {
    unsigned int sleepTimePerDaySeconds; /**< The number of seconds spent asleep today.*/
    unsigned int wakeTimePerDaySeconds; /**< The number of seconds spent awake today.*/
    unsigned int wakeUpsPerDay; /**< The number of wake-ups today.*/
    unsigned int actionsPerDay[MAX_NUM_ACTION_TYPES]; /**< The number of time each action was executed today.*/
    unsigned int energyPerDayUWH; /**< The energy consumed today in uWh.*/
    unsigned int cellularConnectionAttemptsSinceReset; /**< The number of cellular connection attempts since initial power-on.*/
    unsigned int cellularConnectionSuccessSinceReset; /**< The number of successful cellular connections since initial power-on.*/
    unsigned int cellularBytesTransmittedSinceReset; /**< The number of bytes transmitted since initial power-on.*/
    unsigned int cellularBytesReceivedSinceReset; /**< The number of bytes received since initial power-on.*/
    unsigned int positionAttemptsSinceReset; /**< The number of position fix attempts since initial power-on.*/
    unsigned int positionSuccessSinceReset; /**< The number of successful position fixes since initial power-on.*/
    unsigned int positionLastNumSvVisible; /**< The number of space vehicles visible at the last position fix attempt.*/
} DataStatistics;

/** Data struct for a portion of logging.
 * Note: don't make the array size here bigger
 * without figuring out what CODEC_ENCODE_BUFFER_MIN_SIZE
 * (in eh_codec.h) should be as a result, since this
 * is the largest single data item to encode into a JSON
 * structure. You can figure this out by running either the
 * codec or modem unit tests, and grabbing the output
 * when a log item has been encoded.
 */
typedef struct {
    unsigned int logClientVersion; /**< The version of the log client compiled into the target.*/
    unsigned int logApplicationVersion; /**< The version of the application logging compiled into the target.*/
    unsigned int numItems; /**< The number of items in the following array.*/
    LogEntry log[25];
} DataLog;

/** A union of all the possible data structs.
 */
typedef union {
    DataCellular cellular;
    DataHumidity humidity;
    DataAtmosphericPressure atmosphericPressure;
    DataTemperature temperature;
    DataLight light;
    DataAcceleration acceleration;
    DataPosition position;
    DataMagnetic magnetic;
    DataBle ble;
    DataWakeUpReason wakeUpReason;
    DataEnergySource energySource;
    DataStatistics statistics;
    DataLog log;
} DataContents;

/** The possible types of flag in a data
 * item, used as a bitmap.  Order is important;
 * "send now" is higher than "requires ack" is
 * higher than nothing.
 */
typedef enum {
    DATA_FLAG_SEND_NOW = 0x02,
    DATA_FLAG_REQUIRES_ACK = 0x01
} DataFlag;

/** Definition of a data item.
 * Note: order is important here, contents MUST be
 * at the end of the structure as that allows the
 * size that is malloced for this struct to be
 * varied depending on the type.
 */
typedef struct DataTag {
    Action *pAction;
    time_t timeUTC;
    DataType type;
    unsigned char flags;
    DataTag *pPrevious;
    DataTag *pNext;
    DataContents contents;
} Data;

/**************************************************************************
 * FUNCTIONS
 *************************************************************************/

/** Return the difference between a pair of data items.  If a data item
 * consists of many values a decision is taken as to which values to
 * involve; see the implementation of this function for those choices.
 *
 * @param pData1    a pointer to the first data item.
 * @param pData2    a pointer to the second data item.
 *
 * @return          the difference between the two data items.
 */
int dataDifference(const Data *pData1, const Data *pData2);

/** Make a data item, malloc()ing memory as necessary, adding it to the
 * end of the list.
 *
 * @param pAction   The action to which the data is attached (may be NULL).
 * @param type      The data type.
 * @param flags     The bitmap of flags for this data item.
 * @param pContents The content to be copied into the data (may be NULL).
 *
 * @return          A pointer the the malloc()ed data structure of NULL
 *                  on failure.
 */
Data *pDataAlloc(Action *pAction, DataType type, unsigned char flags,
                 const DataContents *pContents);

/** Free a data item, releasing memory and NULLing any pointer to this
 * data from the action list.
 * Note: this has no effect on any action associated with the data,
 * which must be freed separately.
 *
 * @param pData   A pointer to the data pointer to be freed.
 */
void dataFree(Data **ppData);

/** Check if a request to allocate room for the given data type
 * would succeed.
 */
bool dataAllocCheck(DataType type);

/** Return the number of data items stored.
 */
int dataCount();

/** Sort the data list.  The list is sorted in the following order:
 *
 * 1. Items with the flag DATA_FLAG_SEND_NOW in time order, newest first.
 * 2. Items with the flag DATA_FLAG_REQUIRES_ACK in time order, newest first.
 * 3. Everything else in time order, newest first.
 *
 * @return   A pointer to the first entry in the sorted data list,
 *           NULL if there are no entries.
 */
Data *pDataSort();

/** Get a pointer to the first data item.  The data pointer is reset
 * to the top of the list.  This is like pDataSort() list but without
 * the sorting.
 *
 * @return   A pointer to the first entry in the data list,
 *           NULL if there are no entries left.
 */
Data *pDataFirst();

/** Get a pointer to the next data item.  The data pointer is reset
 * to the top of the list when pDataSort() or pDataFirst() is called.
 *
 * @return   A pointer to the next entry in the sorted data list,
 *           NULL if there are no entries left.
 */
Data *pDataNext();

/** Lock the data list.  This may be required by the action
 * module when it is clearing out actions and it may be required
 * when a call to dataAllocCheck() is to be made in a multi-threaded
 * environment and you don't want another thread to grab the space
 * for data. It should not be used in any other circumstance.  Must
 * be followed by a call to dataUnlockList() or no-one is going to
 * get anywhere.
 */
void dataLockList();

/** Unlock the data list.  This may be required by the action
 * module when it is clearing out data. It should not be used
 * by anyone else.
 */
void dataUnlockList();

#endif // _EH_DATA_H_

// End Of File
