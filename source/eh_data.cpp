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

#include <mbed.h> // for MBED_ASSERT
#include <stdlib.h> // for abs()
#include <stddef.h> // for offsetof()
#include <eh_utilities.h> // for LOCK()/UNLOCK()
#include <eh_data.h>

/**************************************************************************
 * MANIFEST CONSTANTS
 *************************************************************************/

/**************************************************************************
 * LOCAL VARIABLES
 *************************************************************************/

/** The size of the data contents for each data type.  Must be completed
 * in the same order as the DataType enum so that it can be indexed with
 * DataType.
 */
static const size_t gSizeOfContents[] = {0, /* DATA_TYPE_NULL */
                                         sizeof(DataCellular), /* DATA_TYPE_CELLULAR */
                                         sizeof(DataHumidity), /* DATA_TYPE_HUMIDITY */
                                         sizeof(DataAtmosphericPressure), /* DATA_TYPE_ATMOSPHERIC_PRESSURE */
                                         sizeof(DataTemperature), /* DATA_TYPE_TEMPERATURE */
                                         sizeof(DataLight), /* DATA_TYPE_LIGHT */
                                         sizeof(DataOrientation), /* DATA_TYPE_ORIENTATION */
                                         sizeof(DataPosition), /* DATA_TYPE_POSITION */
                                         sizeof(DataMagnetic), /* DATA_TYPE_MAGNETIC */
                                         sizeof(DataBle), /* DATA_TYPE_BLE */
                                         sizeof(DataWakeUpReason), /* DATA_TYPE_WAKE_UP_REASON */
                                         sizeof(DataEnergySource), /* DATA_TYPE_ENERGY_SOURCE */
                                         sizeof(DataStatistics), /* DATA_TYPE_STATISTICS */
                                         sizeof(DataLog) /* DATA_TYPE_LOG */};

/**  The root of the data link list.
 */
static Data *gpDataList = NULL;

/** Mutex to protect the data lists.
 */
static Mutex gMtx;

/** A pointer to the next data item.
 */
static Data *gpNextData = NULL;

/**************************************************************************
 * STATIC FUNCTIONS
 *************************************************************************/

// Condition function to return true if pNextData has a higher flag value
// than pData or, if the flags are the same, then return true if
// pNextData is newer (a higher number) than pData.
static bool conditionFlags(Data *pData, Data *pNextData)
{
    return ((pNextData->flags > pData->flags) ||
            ((pNextData->flags == pData->flags) &&
             (pNextData->timeUTC > pData->timeUTC)));
}

// Sort gpDataList using the given condition function.
// NOTE: this does not lock the list.
static void sort(bool condition(Data *, Data *)) {
    Data **ppData = &(gpDataList);
    Data *pNext;
    Data *pTmp;
    Timer timer;

    timer.reset();
    timer.start();
    while ((*ppData != NULL) && ((pNext = (*ppData)->pNext) != NULL) && (timer.read_ms() < DATA_SORT_GUARD_TIMER_MS)) {
        // If condition is true, swap the entries and restart the sort
        if (condition(*ppData, pNext)) {
            pTmp = (*ppData)->pPrevious;
            if (pNext->pNext != NULL) {
                pNext->pNext->pPrevious= *ppData;
            }
            (*ppData)->pNext = pNext->pNext;
            pNext->pPrevious = (*ppData)->pPrevious;
            (*ppData)->pPrevious = pNext;
            pNext->pNext = *ppData;
            if (pTmp != NULL) {
                pTmp->pNext = pNext;
            }
            // If we were at the root, move that pointer too
            if (ppData == &(gpDataList)) {
                gpDataList = pNext;
            }
            ppData = &(gpDataList);
        } else {
            ppData = &((*ppData)->pNext);
        }
    }
    timer.stop();
}

/**************************************************************************
 * PUBLIC FUNCTIONS
 *************************************************************************/

// Return the difference between a pair of data items.
int dataDifference(const Data *pData1, const Data *pData2)
{
    int difference = 0;
    int x;

    MBED_ASSERT(pData1 != NULL);
    MBED_ASSERT(pData2 != NULL);
    MBED_ASSERT(pData1->type == pData2->type);

    switch (pData1->type) {
        case DATA_TYPE_CELLULAR:
            // Cellular is a mix of stuff; we chose to apply the threshold
            // to the RSRP value since that is both a variable and a useful
            // number
            difference = pData1->contents.cellular.rsrpDbm - pData2->contents.cellular.rsrpDbm;
        break;
        case DATA_TYPE_HUMIDITY:
            difference = pData1->contents.humidity.percentage - pData2->contents.humidity.percentage;
        break;
        case DATA_TYPE_ATMOSPHERIC_PRESSURE:
            difference = pData1->contents.atmosphericPressure.pascalX100 - pData2->contents.atmosphericPressure.pascalX100;
        break;
        case DATA_TYPE_TEMPERATURE:
            difference = pData1->contents.temperature.cX100 - pData2->contents.temperature.cX100;
        break;
        case DATA_TYPE_LIGHT:
            // For light use the larger of the lux and UV index values
            difference = pData1->contents.light.lux - pData2->contents.light.lux;
            x = pData1->contents.light.uvIndexX1000 - pData2->contents.light.uvIndexX1000;
            if (abs(x) > abs(difference)) {
                difference = x;
            }
        break;
        case DATA_TYPE_ORIENTATION:
            // For orientation use the largest of the x, y, or z values
            difference = pData1->contents.orientation.x - pData2->contents.orientation.x;
            x = pData1->contents.orientation.y - pData2->contents.orientation.y;
            if (abs(x) > abs(difference)) {
                difference = x;
            }
            x = pData1->contents.orientation.z- pData2->contents.orientation.z;
            if (abs(x) > abs(difference)) {
                difference = x;
            }
        break;
        case DATA_TYPE_POSITION:
            // For position use the largest of lat, long, radius and altitude
            difference = pData1->contents.position.latitudeX10e7 - pData2->contents.position.latitudeX10e7;
            x = pData1->contents.position.longitudeX10e7 - pData2->contents.position.longitudeX10e7;
            if (abs(x) > abs(difference)) {
                difference = x;
            }
            x = pData1->contents.position.radiusMetres - pData2->contents.position.radiusMetres;
            if (abs(x) > abs(difference)) {
                difference = x;
            }
            x = pData1->contents.position.altitudeMetres - pData2->contents.position.altitudeMetres;
            if (abs(x) > abs(difference)) {
                difference = x;
            }
        break;
        case DATA_TYPE_MAGNETIC:
            difference = pData1->contents.magnetic.teslaX1000 - pData2->contents.magnetic.teslaX1000;
        break;
        case DATA_TYPE_BLE:
            difference = pData1->contents.ble.batteryPercentage - pData2->contents.ble.batteryPercentage;
        break;
        case DATA_TYPE_WAKE_UP_REASON:
            // Deliberate fall-through
        case DATA_TYPE_ENERGY_SOURCE:
            // Deliberate fall-through
        case DATA_TYPE_STATISTICS:
            // Deliberate fall-through
        case DATA_TYPE_LOG:
            difference = 1;
            // For all of these return 1 as they are not measurements,
            // simply for management purposes
        break;
        default:
            MBED_ASSERT(false);
        break;
    }

    return difference;
}

// Make a data item, malloc()ing memory as necessary and adding it to
// the end of the data linked list.
Data *pDataAlloc(Action *pAction, DataType type, unsigned char flags,
                 const DataContents *pContents)
{
    Data **ppThis;
    Data *pPrevious;
    int x = 0;

    LOCK(gMtx);

    MBED_ASSERT(type < MAX_NUM_DATA_TYPES);
    MBED_ASSERT(pContents != NULL);

    // Find the end of the data list
    pPrevious = NULL;
    ppThis = &(gpDataList);
    while (*ppThis != NULL) {
        pPrevious = *ppThis;
        ppThis = &((*ppThis)->pNext);
        x++;
    }

    // Allocate room for the data
    *ppThis = (Data *) malloc(offsetof(Data, contents) + gSizeOfContents[type]);
    if (*ppThis != NULL) {
        // Copy in the data values
        (*ppThis)->timeUTC = time(NULL);
        (*ppThis)->type = type;
        (*ppThis)->flags = flags;
        (*ppThis)->pAction = pAction;
        memcpy(&((*ppThis)->contents), pContents, gSizeOfContents[type]);
        (*ppThis)->pPrevious = pPrevious;
        if ((*ppThis)->pPrevious != NULL) {
            (*ppThis)->pPrevious->pNext = (*ppThis);
        }
        (*ppThis)->pNext = NULL;
    }

    if (pAction != NULL) {
        pAction->pData = *ppThis;
    }

    UNLOCK(gMtx);

    return *ppThis;
}

// Remove a data item, free()ing memory.
void dataFree(Data **ppData)
{
    Data **ppThis;
    Data *pNext;

    LOCK(gMtx);

    if ((ppData != NULL) && ((*ppData) != NULL)) {
        // Check that we have a valid pointer by finding it in the list
        ppThis = &(gpDataList);
        while ((*ppThis != NULL) && (*ppThis != *ppData)) {
            ppThis = &((*ppThis)->pNext);
        }

        if (*ppThis != NULL) {
            actionLockList();
            // Find the action that is pointing at this
            // data item and set its data pointer to NULL
            if ((*ppData)->pAction != NULL) {
                (*ppData)->pAction->pData = NULL;
            }
            actionUnlockList();

            // Seal up the list
            if ((*ppData)->pPrevious != NULL) {
                ((*ppData)->pPrevious)->pNext = (*ppData)->pNext;
            }
            // In case we're at the root or gpNextData, remember
            // where the next item is
            pNext = (*ppData)->pNext;
            if ((*ppData)->pNext != NULL) {
                (*ppData)->pNext->pPrevious = (*ppData)->pPrevious;
            }
            // Free this item
            free (*ppData);
            // If we were at the root, reconnect the root with the
            // next item in the list
            if (ppThis == &(gpDataList)) {
                *ppThis = pNext;
            }
            // If gpNextData was pointing at this item, move it
            // on to the next one
            if (*ppThis == gpNextData) {
                gpNextData = pNext;
            }

            *ppData = NULL;
        }
    }

    UNLOCK(gMtx);
}

// Sort the data list.
Data *pDataSort()
{
    LOCK(gMtx);

    sort(conditionFlags);
    gpNextData = gpDataList;

    UNLOCK(gMtx);

    return gpNextData;
}

// Get a pointer to the first data item.
Data *pDataFirst()
{
    gpNextData = gpDataList;

    return gpNextData;
}

// Get a pointer to the next data item.
Data *pDataNext()
{
    LOCK(gMtx);

    if (gpNextData != NULL) {
        gpNextData = gpNextData->pNext;
    }

    UNLOCK(gMtx);

    return gpNextData;
}

// Lock the data list.
void dataLockList()
{
    gMtx.lock();
}

// Unlock the data list.
void dataUnlockList()
{
    gMtx.unlock();
}

// End of file
