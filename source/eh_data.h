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
    DATA_TYPE_ORIENTATION,
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
    int rsrpDbm;
    int rssiDbm;
    int rsrq;
    int snrDbm;
    int eclDbm;
    unsigned int physicalCellId;
    unsigned int pci;
    int transmitPowerDbm;
    unsigned int earfcn;
} DataCellular;

/** Data struct for humidity.
 */
typedef struct {
    unsigned char percentage;
} DataHumidity;

/** Data struct for atmospheric pressure.
 */
typedef struct {
    unsigned int pascalX100;
} DataAtmosphericPressure;

/** Data struct for temperature.
 */
typedef struct {
    signed int cX100;
} DataTemperature;

/** Data struct for light.
 */
typedef struct {
    int lux;
    int uvIndexX1000;
} DataLight;

/** Data struct for orientation.
 */
typedef struct {
    int x;
    int y;
    int z;
} DataOrientation;

/** Data struct for position.
 */
typedef struct {
    int latitudeX10e7;
    int longitudeX10e7;
    int radiusMetres;
    int altitudeMetres;
    unsigned char speedMPS;
} DataPosition;

/** Data struct for the hall effect sensor.
 */
typedef struct {
    unsigned int teslaX1000;
} DataMagnetic;

/** Data struct for BLE.
 */
typedef struct {
    char name[DATA_MAX_LEN_BLE_DEVICE_NAME];
    unsigned char batteryPercentage;
} DataBle;

/** The wake-up reasons.
 * Note: if you modify this then also modify
 * gpWakeUpReason in eh_codec.cpp.
 */
typedef enum {
    WAKE_UP_RTC,
    WAKE_UP_ORIENTATION,
    WAKE_UP_MAGNETIC,
    MAX_NUM_WAKE_UP_REASONS
} WakeUpReason;

/** Data struct for wake-up reason.
 */
typedef struct {
    WakeUpReason wakeUpReason;
} DataWakeUpReason;

/** Data struct for energy source.
 */
typedef struct {
    unsigned char x;
} DataEnergySource;

/** Data struct for statistics.
 */
typedef struct {
    unsigned int sleepTimePerDaySeconds;
    unsigned int wakeTimePerDaySeconds;
    unsigned int wakeUpsPerDay;
    unsigned int actionsPerDay[MAX_NUM_ACTION_TYPES];
    unsigned int energyPerDayUWH;
    unsigned int cellularConnectionAttemptsSinceReset;
    unsigned int cellularConnectionSuccessSinceReset;
    unsigned int cellularBytesTransmittedSinceReset;
    unsigned int cellularBytesReceivedSinceReset;
    unsigned int positionAttemptsSinceReset;
    unsigned int positionSuccessSinceReset;
    unsigned int positionLastNumSvVisible;
} DataStatistics;

/** Data struct for a portion of logging.
 * Note: don't make the array size here bigger
 * without figuring out what CODEC_ENCODE_BUFFER_MIN_SIZE
 * (in eh_codec.h) should be as a result, since this
 * is the largest single data item to encode into a JSON
 * structure.
 */
typedef struct {
    unsigned int numItems;
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
    DataOrientation orientation;
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
 * @param pContents The content to be copied into the data.
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
 * module when it is clearing out actions. It should not be used
 * by anyone else.  Must be followed by a call to dataUnlockList()
 * or no-one is going to get anywhere.
 */
void dataLockList();

/** Unlock the data list.  This may be required by the action
 * module when it is clearing out data. It should not be used
 * by anyone else.
 */
void dataUnlockList();

#endif // _EH_DATA_H_

// End Of File
