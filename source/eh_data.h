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
#include <eh_action.h>

/**************************************************************************
 * MANIFEST CONSTANTS
 *************************************************************************/

/**************************************************************************
 * TYPES
 *************************************************************************/

/** The types of data.  If you add a new item here, don't forget to:
 *
 *   - add a struct for it,
 *   - add that struct to the DataContents union,
 *   - add an entry for it in sizeOfContents[],
 *   - update dataVariability() and pMakeData() to handle it,
 *   - update the unit tests to be aware of it.
 *
 * Note: order is important, don't change this unless you also change
 * sizeOfContents[] to match.
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
    int physicalCellId;
    int pci;
    int transmitPowerDbm;
    int earfcn;
} DataCellular;

/** Data struct for humidity.
 */
typedef struct {
    unsigned char percentage;
} DataHumidity;

/** Data struct for atmospheric pressure.
 */
typedef struct {
    unsigned int pascal;
} DataAtmosphericPressure;

/** Data struct for temperature.
 */
typedef struct {
    signed char c;
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
    int latitudeX1000;
    int longitudeX1000;
    int radiusMetres;
    int altitudeMetres;
    unsigned char speed;
} DataPosition;

/** Data struct for the hall effect sensor.
 */
typedef struct {
    unsigned int microTesla;
} DataMagnetic;

/** Data struct for BLE.
 */
typedef struct {
    int x;
    int y;
    int z;
    unsigned char batteryPercentage;
} DataBle;

/** The wake-up reasons.
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
    int sleepTimePerDaySeconds;
    int wakeTimePerDaySeconds;
    int wakeUpsPerDay;
    int actionsPerDay[MAX_NUM_ACTION_TYPES];
    int energyPerDayUWH;
    int cellularRegistrationAttemptsSinceReset;
    int cellularRegistrationSuccessSinceReset;
    int cellularDataTransferAttemptsSinceReset;
    int cellularDataTransferSuccessSinceReset;
    int cellularBytesTransmittedSinceReset;
    int cellularBytesReceivedSinceReset;
    int locationAttemptsSinceReset;
    int locationSuccessSinceReset;
    int locationLastNumSvVisible;
} DataStatistics;

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
} DataContents;

/** Definition of a data item.
 * Note: order is important here, contents MUST be
 * at the end of the structure as that allows the
 * size that is malloced for this struct to be
 * varied depending on the type.
 */
typedef struct {
    Action *pAction;
    time_t timeUTC;
    DataType type;
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
int dataDifference(Data *pData1, Data *pData2);

/** Make a data item, malloc()ing memory as necessary.  It is up to the caller
 * to free the memory when done.
 *
 * @param pAction   The action to which the data is attached.
 * @param type      The data type.
 * @param pContents The content to be copied into the data.
 *
 * @return          A pointer the the malloc()ed data structure of NULL
 *                  on failure.
 */

Data *pMakeData(Action *pAction, DataType type, DataContents *pContents);

#endif // _EH_DATA_H_

// End Of File
