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

#include <eh_data.h>
#include <eh_codec.h>
#include <eh_utilities.h> // For ARRAY_SIZE

/* The encoded data will look something like this:
 *
 * {
 *    "i":0,"r":{
 *        "loc":{
 *            "t":1527172040,"uWh":134,
 *            "d":{
 *                "lat":52.223117,"lng":-0.074391,"rad":5,"alt":65,"spd":0
 *            }
 *        },
 *        "lux":{
 *            "t":1527172340,"uWh":597,
 *            "d":{
 *                "lux":1004
 *            }
 *        }
 *    }
 * }
 *
 * ...where:
 *
 * i is the index number of this report.
 */

/**************************************************************************
 * MANIFEST CONSTANTS
 *************************************************************************/

// Advance the buffer pointer and decrement the length based on
// x and add the number of bytes encoded to t
#define ADVANCE_BUFFER(pBuf, len, x, t) {len -= x; pBuf += x; t += x;}

// Rewind the buffer pointer and increment the length based on
// x and remove the number of bytes encoded fro t
#define REWIND_BUFFER(pBuf, len, x, t) {len += x; pBuf -= x; t -= x;}

/**************************************************************************
 * LOCAL VARIABLES
 *************************************************************************/

/** The report index.
 */
static int gReportIndex = 0;

/** The bracing depth we are at in an encoded message.
 */
static int gBraceDepth = 0;

/** The possible wake-up reasons as text.
 */
static const char *gpWakeUpReason[] = {"RTC", "ORI", "MAG"};

/**************************************************************************
 * STATIC FUNCTIONS
 *************************************************************************/

/** Encode the index part of a report, i.e.: |{"i":xxx|
 */
static int encodeIndex(char *pBuf, int len)
{
    int bytesEncoded = -1;
    int x;

    // Attempt to snprintf() the string
    x = snprintf(pBuf, len, "{\"i\":%d", gReportIndex);
    if ((x > 0) && (x < len)) {// x < len since snprintf() adds a terminator
        bytesEncoded = x;      // but doesn't count it
        gBraceDepth++;
    }

    return bytesEncoded;
}

/** Encode the report start, i.e.: |,"r":{|
 */
static int encodeReportStart(char *pBuf, int len)
{
    int bytesEncoded = -1;
    int x;

    // Attempt to snprintf() the string
    x = snprintf(pBuf, len, ",\"r\":{");
    if ((x > 0) && (x < len)) {// x < len since snprintf() adds a terminator
        bytesEncoded = x;      // but doesn't count it
        gBraceDepth++;
    }

    return bytesEncoded;
}

/** Encode the opening fields of a data item, e.g.: |"pos":{"t":xxxxxxx,"uWh":xxx|
 */
static int encodeDataHeader(char *pBuf, int len, const char *pPrefix, time_t timeUTC,
                            unsigned int energyCostUWH)
{
    int bytesEncoded = -1;
    int x;

    // Attempt to snprintf() the string
    x = snprintf(pBuf, len, "\"%s\":{\"t\":%d,\"uWh\":%d", pPrefix, (int) timeUTC, energyCostUWH);
    if ((x > 0) && (x < len)) {// x < len since snprintf() adds a terminator
        bytesEncoded = x;      // but doesn't count it
        gBraceDepth++;
    }

    return bytesEncoded;
}

/** Encode a cellular data item: |,"d":{"rsrp":-70,"rssi":-75,"rsrq":5,"snr":-5,"ecl":-80,"phyci":155,"pci":14,"tpw":21,"ch":12412}|
 */
static int encodeDataCellular(char *pBuf, int len, DataCellular *pData)
{
    int bytesEncoded = -1;
    int x;

    // Attempt to snprintf() the string
    x = snprintf(pBuf, len, ",\"d\":{\"rsrp\":%d,\"rssi\":%d,\"rsrq\":%d,"
                 "\"snr\":%d,\"ecl\":%d,\"phyci\":%u,\"pci\":%u,\"tpw\":%d,\"ch\":%u}",
                 pData->rsrpDbm, pData->rssiDbm, pData->rsrq, pData->snrDbm,
                 pData->eclDbm, pData->physicalCellId, pData->pci,
                 pData->transmitPowerDbm, pData->earfcn);
    if ((x > 0) && (x < len)) {// x < len since snprintf() adds a terminator
        bytesEncoded = x;      // but doesn't count it
    }

    return bytesEncoded;
}

/** Encode a humidity data item: |,"d":{"hum":70}|
 */
static int encodeDataHumidity(char *pBuf, int len, DataHumidity *pData)
{
    int bytesEncoded = -1;
    int x;

    // Attempt to snprintf() the string
    x = snprintf(pBuf, len, ",\"d\":{\"hum\":%d}", pData->percentage);
    if ((x > 0) && (x < len)) {// x < len since snprintf() adds a terminator
        bytesEncoded = x;      // but doesn't count it
    }

    return bytesEncoded;
}

/** Encode an atmospheric pressure data item: |,"d":{"apr":90000}|
 */
static int encodeDataAtmosphericPressure(char *pBuf, int len, DataAtmosphericPressure *pData)
{
    int bytesEncoded = -1;
    int x;

    // Attempt to snprintf() the string
    x = snprintf(pBuf, len, ",\"d\":{\"apr\":%u}", pData->pascalX100);
    if ((x > 0) && (x < len)) {// x < len since snprintf() adds a terminator
        bytesEncoded = x;      // but doesn't count it
    }

    return bytesEncoded;
}

/** Encode a temperature data item: |,"d":{"tmp":2300}|
 */
static int encodeDataTemperature(char *pBuf, int len, DataTemperature *pData)
{
    int bytesEncoded = -1;
    int x;

    // Attempt to snprintf() the string
    x = snprintf(pBuf, len, ",\"d\":{\"apr\":%u}", pData->cX100);
    if ((x > 0) && (x < len)) {// x < len since snprintf() adds a terminator
        bytesEncoded = x;      // but doesn't count it
    }

    return bytesEncoded;
}

/** Encode a light data item: |,"d":{"lux":4500,"uvi":3000}|
 */
static int encodeDataLight(char *pBuf, int len, DataLight *pData)
{
    int bytesEncoded = -1;
    int x;

    // Attempt to snprintf() the string
    x = snprintf(pBuf, len, ",\"d\":{\"lux\":%u,\"uvi\":%u}", pData->lux,
                 pData->uvIndexX1000);
    if ((x > 0) && (x < len)) {// x < len since snprintf() adds a terminator
        bytesEncoded = x;      // but doesn't count it
    }

    return bytesEncoded;
}

/** Encode an orientation data item: |,"d":{"x":45,"y":125,"z":-30}|
 */
static int encodeDataOrientation(char *pBuf, int len, DataOrientation *pData)
{
    int bytesEncoded = -1;
    int x;

    // Attempt to snprintf() the string
    x = snprintf(pBuf, len, ",\"d\":{\"x\":%d,\"y\":%d,\"z\":%d}", pData->x,
                 pData->y, pData->z);
    if ((x > 0) && (x < len)) {// x < len since snprintf() adds a terminator
        bytesEncoded = x;      // but doesn't count it
    }

    return bytesEncoded;
}

/** Encode a position data item: |,"d":{"lat":52223117,"lng":-0074391,"rad":5,"alt":65,"spd":0}|
 */
static int encodeDataPosition(char *pBuf, int len, DataPosition *pData)
{
    int bytesEncoded = -1;
    int x;

    // Attempt to snprintf() the string
    x = snprintf(pBuf, len, ",\"d\":{\"lat\":%d,\"lng\":%d,\"rad\":%d,\"alt\":%d,\"spd\":%u}",
                 pData->latitudeX10e7, pData->longitudeX10e7, pData->radiusMetres,
                 pData->altitudeMetres, pData->speedMPS);
    if ((x > 0) && (x < len)) {// x < len since snprintf() adds a terminator
        bytesEncoded = x;      // but doesn't count it
    }

    return bytesEncoded;
}

/** Encode a magnetic data item: |,"d":{"tla":1500}|
 */
static int encodeDataMagnetic(char *pBuf, int len, DataMagnetic *pData)
{
    int bytesEncoded = -1;
    int x;

    // Attempt to snprintf() the string
    x = snprintf(pBuf, len, ",\"d\":{\"tla\":%u}", pData->teslaX1000);
    if ((x > 0) && (x < len)) {// x < len since snprintf() adds a terminator
        bytesEncoded = x;      // but doesn't count it
    }

    return bytesEncoded;
}

/** Encode a BLE data item: |,"d":{"dev":"NINA-B1:354","bat":89}|
 */
static int encodeDataBle(char *pBuf, int len, DataBle *pData)
{
    int bytesEncoded = -1;
    int x;

    // Attempt to snprintf() the string
    x = snprintf(pBuf, len, ",\"d\":{\"dev\":\"%s\",\"bat\":%u}", pData->name,
                 pData->batteryPercentage);
    if ((x > 0) && (x < len)) {// x < len since snprintf() adds a terminator
        bytesEncoded = x;      // but doesn't count it
    }

    return bytesEncoded;
}

/** Encode a wake-up reason data item: |,"d":{"wkp":"rtc"}|
 */
static int encodeDataWakeUpReason(char *pBuf, int len, DataWakeUpReason *pData)
{
    int bytesEncoded = -1;
    int x;

    // Attempt to snprintf() the string
    x = snprintf(pBuf, len, ",\"d\":{\"wkp\":\"%s\"}", gpWakeUpReason[pData->wakeUpReason]);
    if ((x > 0) && (x < len)) {// x < len since snprintf() adds a terminator
        bytesEncoded = x;      // but doesn't count it
    }

    return bytesEncoded;
}

/** Encode an energy source data item: |,"d":{"nrg":2}|
 */
static int encodeDataEnergySource(char *pBuf, int len, DataEnergySource *pData)
{
    int bytesEncoded = -1;
    int x;

    // Attempt to snprintf() the string
    x = snprintf(pBuf, len, ",\"d\":{\"nrg\":%u}", pData->x);
    if ((x > 0) && (x < len)) {// x < len since snprintf() adds a terminator
        bytesEncoded = x;      // but doesn't count it
    }

    return bytesEncoded;
}

/** Encode a statistics data item: |,"d":{"stpd":25504,"wtpd":1455,"wpd":45,"apd":[5,4,6,2,5,6,2,0],"epd":7800,
 *                                  "cra":67,"crs":65,"cda":65,"cds":60,"cbt":352352,"cbr":252,"poa":40","pos":5,"svs":4}|
 */
static int encodeDataStatistics(char *pBuf, int len, DataStatistics *pData)
{
    int bytesEncoded = -1;
    bool keepGoing = true;
    int x;
    int total = 0;

    // Attempt to snprintf() the first portion of the string
    x = snprintf(pBuf, len, ",\"d\":{\"stpd\":%u,\"wtpd\":%u,\"wpd\":%u,\"apd\":",
                 pData->sleepTimePerDaySeconds, pData->wakeTimePerDaySeconds,
                 pData->wakeUpsPerDay);
    if ((x > 0) && (x < len)) {               // x < len since snprintf() adds a terminator
        ADVANCE_BUFFER(pBuf, len, x, total);  // but doesn't count it
        // Add the array of action counts per day
        for (unsigned int y = 0; keepGoing && (y < ARRAY_SIZE(pData->actionsPerDay)); y++) {
            x = snprintf(pBuf, len, "[%u,", pData->actionsPerDay[y]);
            if ((x > 0) && (x < len)) {               // x < len since snprintf() adds a terminator
                ADVANCE_BUFFER(pBuf, len, x, total);  // but doesn't count it
            } else {
                keepGoing = false;
            }
        }
        if (keepGoing) {
            // Replace the last comma with a closing square bracket
            *(pBuf - 1) = ']';
            //  Now add the last portion of the string
            x = snprintf(pBuf, len, ",\"epd\":%u,\"cra\":%u,\"crs\":%u,\"cda\":%u,\"cds\":%u,"
                         "\"cbt\":%u,\"cbr\":%u,\"poa\":%u,\"pos\":%u,\"svs\":%u}",
                         pData->energyPerDayUWH, pData->cellularRegistrationAttemptsSinceReset,
                         pData->cellularRegistrationSuccessSinceReset,
                         pData->cellularDataTransferAttemptsSinceReset,
                         pData->cellularDataTransferSuccessSinceReset,
                         pData->cellularBytesTransmittedSinceReset,
                         pData->cellularBytesReceivedSinceReset,
                         pData->positionAttemptsSinceReset,
                         pData->positionAttemptsSinceReset,
                         pData->positionLastNumSvVisible);
            if ((x > 0) && (x < len)) {   // x < len since snprintf() adds a terminator
                bytesEncoded = x + total; // but doesn't count it
            }
        }
    }

    return bytesEncoded;
}

/** Encode a log data item: |,"d":{"log":[[235825,4,1],[235827,5,0]]}|
 */
static int encodeDataLog(char *pBuf, int len, DataLog *pData)
{
    int bytesEncoded = -1;
    bool keepGoing = true;
    int x;
    unsigned int y;
    int total = 0;

    // Attempt to snprintf() the prefix
    x = snprintf(pBuf, len, ",\"d\":{\"log\":[");
    if ((x > 0) && (x < len)) {               // x < len since snprintf() adds a terminator
        ADVANCE_BUFFER(pBuf, len, x, total);  // but doesn't count it
        for (y = 0; keepGoing && (y < ARRAY_SIZE(pData->log)); y++) {
            x = snprintf(pBuf, len, "[%u,%u,%u],", pData->log->timestamp,
                         pData->log->event, pData->log->parameter);
            if ((x > 0) && (x < len)) {               // x < len since snprintf() adds a terminator
                ADVANCE_BUFFER(pBuf, len, x, total);  // but doesn't count it
            } else {
                keepGoing = false;
            }
        }
        if (keepGoing) {
            if (y > 0) {
                // Replace the last comma with a closing square bracket
                *(pBuf - 1) = ']';
                bytesEncoded = total;
            } else {
                // If we never went around the loop, just add a closing bracket
                x = snprintf(pBuf, len, "]");
                if ((x > 0) && (x < len)) {
                    bytesEncoded = x + total;
                }
            }
        }
    }

    return bytesEncoded;
}

/** Encode a single character, decrementing the
 * brace count if it is a closing brace.
 */
static int encodeCharacter(char *pBuf, int len, char character)
{
    int bytesEncoded = -1;

    if (len > 0) {
        *pBuf = character;
        bytesEncoded = 1;
        if ((character == '}') && (gBraceDepth > 0)) {
            gBraceDepth--;
        }
    }

    return bytesEncoded;
}

/**************************************************************************
 * PUBLIC FUNCTIONS
 *************************************************************************/

// Encode queued data into a buffer.
int codecEncodeData(char *pBuf, int len)
{
    int bytesEncoded = 0;
    int bytesEncodedThisDataItem = 0;
    char *pLastEndPoint;
    int lastBraceDepth;
    Data *pData;
    unsigned int energyCostUWH;
    int x;
    bool needComma = false;

    // Encode the index
    x = encodeIndex(pBuf, len);
    if (x > 0) {
        ADVANCE_BUFFER(pBuf, len, x, bytesEncoded);
        // If there was room for that, and there
        // is enough room to close the JSON braces,
        // sort the data and see if there is anything to encode
        if (len >= gBraceDepth) {
            pData = pDataSort();
            if (pData != NULL) {
                // There is data to send, so code the report start
                x = encodeReportStart(pBuf, len);
                if (x > 0) {
                    ADVANCE_BUFFER(pBuf, len, x, bytesEncoded);
                    if (len >= gBraceDepth) {
                        // If there was enough room for that,
                        // and the closing braces, add the data items
                        pLastEndPoint = pBuf;
                        lastBraceDepth = gBraceDepth;
                        while ((bytesEncodedThisDataItem >= 0) && (pData != NULL)) {
                            // Need to make sure we have room for whole
                            // data items here, so keep count with
                            // bytesEncodedThisDataItem and only
                            // add that to the total if the data
                            // item is complete.  Set bytesEncodedThisDataItem
                            // to -1 if we have to forget the data item
                            // we are encoding and exit
                            if (needComma) {
                                x = encodeCharacter(pBuf, len, ',');
                                if (x > 0) {
                                    ADVANCE_BUFFER(pBuf, len, x, bytesEncodedThisDataItem);
                                    if (len < gBraceDepth) {
                                        bytesEncodedThisDataItem = -1;
                                    }
                                } else {
                                    bytesEncodedThisDataItem = -1;
                                }
                            }
                            if (bytesEncodedThisDataItem >= 0) {
                                // Grab the energy cost from the associated action
                                energyCostUWH = 0;
                                if (pData->pAction != NULL) {
                                    energyCostUWH = pData->pAction->energyCostUWH;
                                }
                                switch (pData->type) {
                                    case DATA_TYPE_CELLULAR:
                                        x = encodeDataHeader(pBuf, len, "cel", pData->timeUTC,
                                                             energyCostUWH);
                                        if (x > 0) {
                                            ADVANCE_BUFFER(pBuf, len, x, bytesEncodedThisDataItem);
                                            if (len >= gBraceDepth) {
                                                x = encodeDataCellular(pBuf, len, &pData->contents.cellular);
                                                if (x > 0) {
                                                    ADVANCE_BUFFER(pBuf, len, x, bytesEncodedThisDataItem);
                                                    if (len < gBraceDepth) {
                                                        bytesEncodedThisDataItem= -1;
                                                    }
                                                } else {
                                                    bytesEncodedThisDataItem= -1;
                                                }
                                            } else {
                                                bytesEncodedThisDataItem= -1;
                                            }
                                        } else {
                                            bytesEncodedThisDataItem= -1;
                                        }
                                    break;
                                    case DATA_TYPE_HUMIDITY:
                                        x = encodeDataHeader(pBuf, len, "hum", pData->timeUTC,
                                                             energyCostUWH);
                                        if (x > 0) {
                                            ADVANCE_BUFFER(pBuf, len, x, bytesEncodedThisDataItem);
                                            if (len >= gBraceDepth) {
                                                x = encodeDataHumidity(pBuf, len, &pData->contents.humidity);
                                                if (x > 0) {
                                                    ADVANCE_BUFFER(pBuf, len, x, bytesEncodedThisDataItem);
                                                    if (len < gBraceDepth) {
                                                        bytesEncodedThisDataItem= -1;
                                                    }
                                                } else {
                                                    bytesEncodedThisDataItem= -1;
                                                }
                                            } else {
                                                bytesEncodedThisDataItem= -1;
                                            }
                                        } else {
                                            bytesEncodedThisDataItem= -1;
                                        }
                                    break;
                                    case DATA_TYPE_ATMOSPHERIC_PRESSURE:
                                        x = encodeDataHeader(pBuf, len, "pre", pData->timeUTC,
                                                             energyCostUWH);
                                        if (x > 0) {
                                            ADVANCE_BUFFER(pBuf, len, x, bytesEncodedThisDataItem);
                                            if (len >= gBraceDepth) {
                                                x = encodeDataAtmosphericPressure(pBuf, len,
                                                                                  &pData->contents.atmosphericPressure);
                                                if (x > 0) {
                                                    ADVANCE_BUFFER(pBuf, len, x, bytesEncodedThisDataItem);
                                                    if (len < gBraceDepth) {
                                                        bytesEncodedThisDataItem= -1;
                                                    }
                                                } else {
                                                    bytesEncodedThisDataItem= -1;
                                                }
                                            } else {
                                                bytesEncodedThisDataItem= -1;
                                            }
                                        } else {
                                            bytesEncodedThisDataItem= -1;
                                        }
                                    break;
                                    case DATA_TYPE_TEMPERATURE:
                                        x = encodeDataHeader(pBuf, len, "tmp", pData->timeUTC,
                                                             energyCostUWH);
                                        if (x > 0) {
                                            ADVANCE_BUFFER(pBuf, len, x, bytesEncodedThisDataItem);
                                            if (len >= gBraceDepth) {
                                                x = encodeDataTemperature(pBuf, len, &pData->contents.temperature);
                                                if (x > 0) {
                                                    ADVANCE_BUFFER(pBuf, len, x, bytesEncodedThisDataItem);
                                                    if (len < gBraceDepth) {
                                                        bytesEncodedThisDataItem= -1;
                                                    }
                                                } else {
                                                    bytesEncodedThisDataItem= -1;
                                                }
                                            } else {
                                                bytesEncodedThisDataItem= -1;
                                            }
                                        } else {
                                            bytesEncodedThisDataItem= -1;
                                        }
                                    break;
                                    case DATA_TYPE_LIGHT:
                                        x = encodeDataHeader(pBuf, len, "lgt", pData->timeUTC,
                                                             energyCostUWH);
                                        if (x > 0) {
                                            ADVANCE_BUFFER(pBuf, len, x, bytesEncodedThisDataItem);
                                            if (len >= gBraceDepth) {
                                                x = encodeDataLight(pBuf, len, &pData->contents.light);
                                                if (x > 0) {
                                                    ADVANCE_BUFFER(pBuf, len, x, bytesEncodedThisDataItem);
                                                    if (len < gBraceDepth) {
                                                        bytesEncodedThisDataItem= -1;
                                                    }
                                                } else {
                                                    bytesEncodedThisDataItem= -1;
                                                }
                                            } else {
                                                bytesEncodedThisDataItem= -1;
                                            }
                                        } else {
                                            bytesEncodedThisDataItem= -1;
                                        }
                                    break;
                                    case DATA_TYPE_ORIENTATION:
                                        x = encodeDataHeader(pBuf, len, "ori", pData->timeUTC,
                                                             energyCostUWH);
                                        if (x > 0) {
                                            ADVANCE_BUFFER(pBuf, len, x, bytesEncodedThisDataItem);
                                            if (len >= gBraceDepth) {
                                                x = encodeDataOrientation(pBuf, len, &pData->contents.orientation);
                                                if (x > 0) {
                                                    ADVANCE_BUFFER(pBuf, len, x, bytesEncodedThisDataItem);
                                                    if (len < gBraceDepth) {
                                                        bytesEncodedThisDataItem= -1;
                                                    }
                                                } else {
                                                    bytesEncodedThisDataItem= -1;
                                                }
                                            } else {
                                                bytesEncodedThisDataItem= -1;
                                            }
                                        } else {
                                            bytesEncodedThisDataItem= -1;
                                        }
                                    break;
                                    case DATA_TYPE_POSITION:
                                        x = encodeDataHeader(pBuf, len, "pos", pData->timeUTC,
                                                             energyCostUWH);
                                        if (x > 0) {
                                            ADVANCE_BUFFER(pBuf, len, x, bytesEncodedThisDataItem);
                                            if (len >= gBraceDepth) {
                                                x = encodeDataPosition(pBuf, len, &pData->contents.position);
                                                if (x > 0) {
                                                    ADVANCE_BUFFER(pBuf, len, x, bytesEncodedThisDataItem);
                                                    if (len < gBraceDepth) {
                                                        bytesEncodedThisDataItem= -1;
                                                    }
                                                } else {
                                                    bytesEncodedThisDataItem= -1;
                                                }
                                            } else {
                                                bytesEncodedThisDataItem= -1;
                                            }
                                        } else {
                                            bytesEncodedThisDataItem= -1;
                                        }
                                    break;
                                    case DATA_TYPE_MAGNETIC:
                                        x = encodeDataHeader(pBuf, len, "mag", pData->timeUTC,
                                                             energyCostUWH);
                                        if (x > 0) {
                                            ADVANCE_BUFFER(pBuf, len, x, bytesEncodedThisDataItem);
                                            if (len >= gBraceDepth) {
                                                x = encodeDataMagnetic(pBuf, len, &pData->contents.magnetic);
                                                if (x > 0) {
                                                    ADVANCE_BUFFER(pBuf, len, x, bytesEncodedThisDataItem);
                                                    if (len < gBraceDepth) {
                                                        bytesEncodedThisDataItem= -1;
                                                    }
                                                } else {
                                                    bytesEncodedThisDataItem= -1;
                                                }
                                            } else {
                                                bytesEncodedThisDataItem= -1;
                                            }
                                        } else {
                                            bytesEncodedThisDataItem= -1;
                                        }
                                    break;
                                    case DATA_TYPE_BLE:
                                        x = encodeDataHeader(pBuf, len, "ble", pData->timeUTC,
                                                             energyCostUWH);
                                        if (x > 0) {
                                            ADVANCE_BUFFER(pBuf, len, x, bytesEncodedThisDataItem);
                                            if (len >= gBraceDepth) {
                                                x = encodeDataBle(pBuf, len, &pData->contents.ble);
                                                if (x > 0) {
                                                    ADVANCE_BUFFER(pBuf, len, x, bytesEncodedThisDataItem);
                                                    if (len < gBraceDepth) {
                                                        bytesEncodedThisDataItem= -1;
                                                    }
                                                } else {
                                                    bytesEncodedThisDataItem= -1;
                                                }
                                            } else {
                                                bytesEncodedThisDataItem= -1;
                                            }
                                        } else {
                                            bytesEncodedThisDataItem= -1;
                                        }
                                    break;
                                    case DATA_TYPE_WAKE_UP_REASON:
                                        x = encodeDataHeader(pBuf, len, "wkp", pData->timeUTC,
                                                             energyCostUWH);
                                        if (x > 0) {
                                            ADVANCE_BUFFER(pBuf, len, x, bytesEncodedThisDataItem);
                                            if (len >= gBraceDepth) {
                                                x = encodeDataWakeUpReason(pBuf, len, &pData->contents.wakeUpReason);
                                                if (x > 0) {
                                                    ADVANCE_BUFFER(pBuf, len, x, bytesEncodedThisDataItem);
                                                    if (len < gBraceDepth) {
                                                        bytesEncodedThisDataItem= -1;
                                                    }
                                                } else {
                                                    bytesEncodedThisDataItem= -1;
                                                }
                                            } else {
                                                bytesEncodedThisDataItem= -1;
                                            }
                                        } else {
                                            bytesEncodedThisDataItem= -1;
                                        }
                                    break;
                                    case DATA_TYPE_ENERGY_SOURCE:
                                        x = encodeDataHeader(pBuf, len, "nrg", pData->timeUTC,
                                                             energyCostUWH);
                                        if (x > 0) {
                                            ADVANCE_BUFFER(pBuf, len, x, bytesEncodedThisDataItem);
                                            if (len >= gBraceDepth) {
                                                x = encodeDataEnergySource(pBuf, len, &pData->contents.energySource);
                                                if (x > 0) {
                                                    ADVANCE_BUFFER(pBuf, len, x, bytesEncodedThisDataItem);
                                                    if (len < gBraceDepth) {
                                                        bytesEncodedThisDataItem= -1;
                                                    }
                                                } else {
                                                    bytesEncodedThisDataItem= -1;
                                                }
                                            } else {
                                                bytesEncodedThisDataItem= -1;
                                            }
                                        } else {
                                            bytesEncodedThisDataItem= -1;
                                        }
                                    break;
                                    case DATA_TYPE_STATISTICS:
                                        x = encodeDataHeader(pBuf, len, "sta", pData->timeUTC,
                                                             energyCostUWH);
                                        if (x > 0) {
                                            ADVANCE_BUFFER(pBuf, len, x, bytesEncodedThisDataItem);
                                            if (len >= gBraceDepth) {
                                                x = encodeDataStatistics(pBuf, len, &pData->contents.statistics);
                                                if (x > 0) {
                                                    ADVANCE_BUFFER(pBuf, len, x, bytesEncodedThisDataItem);
                                                    if (len < gBraceDepth) {
                                                        bytesEncodedThisDataItem= -1;
                                                    }
                                                } else {
                                                    bytesEncodedThisDataItem= -1;
                                                }
                                            } else {
                                                bytesEncodedThisDataItem= -1;
                                            }
                                        } else {
                                            bytesEncodedThisDataItem= -1;
                                        }
                                    break;
                                    case DATA_TYPE_LOG:
                                        x = encodeDataHeader(pBuf, len, "log", pData->timeUTC,
                                                             energyCostUWH);
                                        if (x > 0) {
                                            ADVANCE_BUFFER(pBuf, len, x, bytesEncodedThisDataItem);
                                            if (len >= gBraceDepth) {
                                                x = encodeDataLog(pBuf, len, &pData->contents.log);
                                                if (x > 0) {
                                                    ADVANCE_BUFFER(pBuf, len, x, bytesEncodedThisDataItem);
                                                    if (len < gBraceDepth) {
                                                        bytesEncodedThisDataItem= -1;
                                                    }
                                                } else {
                                                    bytesEncodedThisDataItem= -1;
                                                }
                                            } else {
                                                bytesEncodedThisDataItem= -1;
                                            }
                                        } else {
                                            bytesEncodedThisDataItem= -1;
                                        }
                                    break;
                                    default:
                                        MBED_ASSERT(false);
                                    break;
                                }
                                // Encode the closing brace
                                if (bytesEncodedThisDataItem > 0) {
                                    x = encodeCharacter(pBuf, len, '}');
                                    if (x > 0) {
                                        ADVANCE_BUFFER(pBuf, len, x, bytesEncodedThisDataItem);
                                        if (len < gBraceDepth) {
                                            bytesEncodedThisDataItem = -1;
                                        }
                                    } else {
                                        bytesEncodedThisDataItem = -1;
                                    }
                                    // If it was possible to encode a whole data item
                                    // then increment the total byte count and,
                                    // if the item doesn't require an ack, free it
                                    // immediately.
                                    if (bytesEncodedThisDataItem > 0) {
                                        bytesEncoded += bytesEncodedThisDataItem;
                                        bytesEncodedThisDataItem = 0;
                                        pLastEndPoint = pBuf;
                                        lastBraceDepth = gBraceDepth;
                                        if ((pData->flags & DATA_FLAG_REQUIRES_ACK) == 0) {
                                            dataFree(&pData);
                                        }
                                        pData = pDataNext();
                                        needComma = true;
                                    }
                                }
                            }
                        } // While()
                        if (bytesEncodedThisDataItem < 0) {
                            // If we were in the middle of encoding something
                            // that didn't fit then set the buffer and the brace
                            // depth back to the last good point
                            len += pBuf - pLastEndPoint;
                            pBuf = pLastEndPoint;
                            gBraceDepth = lastBraceDepth;
                        }
                    } else {
                        // Not enough room to encode a report plus
                        // closing braces so rewind the buffer
                        REWIND_BUFFER(pBuf, len, x, bytesEncoded);
                    }
                }
                // Add the correct number of closing braces
                while (gBraceDepth > 0) {
                    x = encodeCharacter(pBuf, len, '}');
                    if (x > 0) {
                        ADVANCE_BUFFER(pBuf, len, x, bytesEncoded);
                    }
                }
            }
        } else {
            // Not even enough room to encode the initial
            // index plus its closing brace so rewind the buffer
            REWIND_BUFFER(pBuf, len, x, bytesEncoded);
        }
    }

    return bytesEncoded;
}

// Remove acknowledged data.
void codecAckData()
{
    Data *pData;
    Data *pStop;

    // Since all unacknowledged data will have
    // already been removed, removing the
    // acknowledged data is just a matter
    // of removing everything.
    pStop = pDataNext();
    pData = pDataFirst();
    while (pData != pStop) {
        MBED_ASSERT(pData->flags & DATA_FLAG_REQUIRES_ACK);
        dataFree(&pData);
        pData = pDataNext();
    }
}

// End of file
