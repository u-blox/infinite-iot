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
static const size_t sizeOfContents[] = {0, /* DATA_TYPE_NULL */
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
                                       sizeof(DataStatistics) /* DATA_TYPE_STATISTICS */};

/**************************************************************************
 * STATIC FUNCTIONS
 *************************************************************************/

/**************************************************************************
 * PUBLIC FUNCTIONS
 *************************************************************************/

// Return the difference between a pair of data items.
int dataDifference(Data *pData1, Data *pData2)
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
            difference = pData1->contents.atmosphericPressure.pascal - pData2->contents.atmosphericPressure.pascal;
        break;
        case DATA_TYPE_TEMPERATURE:
            difference = pData1->contents.temperature.c - pData2->contents.temperature.c;
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
            difference = pData1->contents.position.latitudeX1000 - pData2->contents.position.latitudeX1000;
            x = pData1->contents.position.longitudeX1000 - pData2->contents.position.longitudeX1000;
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
            difference = pData1->contents.magnetic.microTesla - pData2->contents.magnetic.microTesla;
        break;
        case DATA_TYPE_BLE:
            // For BLE, use the largest of both orientation and battery level
            difference = pData1->contents.ble.x - pData2->contents.ble.x;
            x = pData1->contents.ble.y - pData2->contents.ble.y;
            if (abs(x) > abs(difference)) {
                difference = x;
            }
            x = pData1->contents.ble.z - pData2->contents.ble.z;
            if (abs(x) > abs(difference)) {
                difference = x;
            }
            x = pData1->contents.ble.batteryPercentage - pData2->contents.ble.batteryPercentage;
            if (abs(x) > abs(difference)) {
                difference = x;
            }
        break;
        case DATA_TYPE_WAKE_UP_REASON:
            // Deliberate fall-through
        case DATA_TYPE_ENERGY_SOURCE:
            // Deliberate fall-through
        case DATA_TYPE_STATISTICS:
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


// Make a data item, malloc()ing memory as necessary.
Data *pDataMake(Action *pAction, DataType type, DataContents *pContents)
{
    Data *pData = NULL;

    MBED_ASSERT(type < MAX_NUM_DATA_TYPES);
    MBED_ASSERT(pContents != NULL);

    pData = (Data *) malloc(offsetof(Data, contents) + sizeOfContents[type]);
    if (pData != NULL) {
        pData->timeUTC = time(NULL);
        pData->type = type;
        pData->pAction = pAction;
        memcpy(&(pData->contents), pContents, sizeOfContents[type]);
    }

    return pData;
}

// End of file
