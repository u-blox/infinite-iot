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
 *    "n":"357520071700641","i":0,"r":{
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
 * n is the name (or ID) of the reporting device.
 * i is the index number of this report.
 * r is the report, see below for the possible contents.
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

/** The point to start in the data list.
 */
static Data *gpData = NULL;

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

/** Encode the index and name part of a report, i.e.: |{"n":"xxx","i":xxx|
 */
static int encodeIndexAndName(char *pBuf, int len, const char *pNameString)
{
    int bytesEncoded = -1;
    int x;

    // Attempt to snprintf() the string
    x = snprintf(pBuf, len, "{\"n\":\"%s\",\"i\":%d", pNameString, gReportIndex);
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
    x = snprintf(pBuf, len, "\"%s\":{\"t\":%d,\"uWh\":%u", pPrefix, (int) timeUTC, energyCostUWH);
    if ((x > 0) && (x < len)) {// x < len since snprintf() adds a terminator
        bytesEncoded = x;      // but doesn't count it
        gBraceDepth++;
    }

    return bytesEncoded;
}

/** Encode a cellular data item: |,"d":{"rsrpdbm":-70,"rssidbm":-75,"rsrq":5,"snrdbm":-5,"ecldbm":-80,"phyci":155,"pci":14,"tpwdbm":21,"ch":12412}|
 */
static int encodeDataCellular(char *pBuf, int len, DataCellular *pData)
{
    int bytesEncoded = -1;
    int x;

    // Attempt to snprintf() the string
    x = snprintf(pBuf, len, ",\"d\":{\"rsrpdbm\":%d,\"rssidbm\":%d,\"rsrq\":%d,"
                 "\"snrdbm\":%d,\"ecldbm\":%d,\"phyci\":%u,\"pci\":%u,\"tpwdbm\":%d,\"ch\":%u}",
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
    x = snprintf(pBuf, len, ",\"d\":{\"%%\":%d}", pData->percentage);
    if ((x > 0) && (x < len)) {// x < len since snprintf() adds a terminator
        bytesEncoded = x;      // but doesn't count it
    }

    return bytesEncoded;
}

/** Encode an atmospheric pressure data item: |,"d":{"pasx100":90000}|
 */
static int encodeDataAtmosphericPressure(char *pBuf, int len, DataAtmosphericPressure *pData)
{
    int bytesEncoded = -1;
    int x;

    // Attempt to snprintf() the string
    x = snprintf(pBuf, len, ",\"d\":{\"pasx100\":%u}", pData->pascalX100);
    if ((x > 0) && (x < len)) {// x < len since snprintf() adds a terminator
        bytesEncoded = x;      // but doesn't count it
    }

    return bytesEncoded;
}

/** Encode a temperature data item: |,"d":{"cx100":2300}|
 */
static int encodeDataTemperature(char *pBuf, int len, DataTemperature *pData)
{
    int bytesEncoded = -1;
    int x;

    // Attempt to snprintf() the string
    x = snprintf(pBuf, len, ",\"d\":{\"cx100\":%u}", pData->cX100);
    if ((x > 0) && (x < len)) {// x < len since snprintf() adds a terminator
        bytesEncoded = x;      // but doesn't count it
    }

    return bytesEncoded;
}

/** Encode a light data item: |,"d":{"lux":4500,"uvix1000":3000}|
 */
static int encodeDataLight(char *pBuf, int len, DataLight *pData)
{
    int bytesEncoded = -1;
    int x;

    // Attempt to snprintf() the string
    x = snprintf(pBuf, len, ",\"d\":{\"lux\":%u,\"uvix1000\":%u}", pData->lux,
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

/** Encode a position data item: |,"d":{"latx10e7":52223117,"lngx10e7":-0074391,"radm":5,"altm":65,"spdmps":0}|
 */
static int encodeDataPosition(char *pBuf, int len, DataPosition *pData)
{
    int bytesEncoded = -1;
    int x;

    // Attempt to snprintf() the string
    x = snprintf(pBuf, len, ",\"d\":{\"latx10e7\":%d,\"lngx10e7\":%d,\"radm\":%d,\"altm\":%d,\"spdmps\":%u}",
                 pData->latitudeX10e7, pData->longitudeX10e7, pData->radiusMetres,
                 pData->altitudeMetres, pData->speedMPS);
    if ((x > 0) && (x < len)) {// x < len since snprintf() adds a terminator
        bytesEncoded = x;      // but doesn't count it
    }

    return bytesEncoded;
}

/** Encode a magnetic data item: |,"d":{"tslx1000":1500}|
 */
static int encodeDataMagnetic(char *pBuf, int len, DataMagnetic *pData)
{
    int bytesEncoded = -1;
    int x;

    // Attempt to snprintf() the string
    x = snprintf(pBuf, len, ",\"d\":{\"tslx1000\":%u}", pData->teslaX1000);
    if ((x > 0) && (x < len)) {// x < len since snprintf() adds a terminator
        bytesEncoded = x;      // but doesn't count it
    }

    return bytesEncoded;
}

/** Encode a BLE data item: |,"d":{"dev":"NINA-B1:354","bat%":89}|
 */
static int encodeDataBle(char *pBuf, int len, DataBle *pData)
{
    int bytesEncoded = -1;
    int x;

    // Attempt to snprintf() the string
    x = snprintf(pBuf, len, ",\"d\":{\"dev\":\"%s\",\"bat%%\":%u}", pData->name,
                 pData->batteryPercentage);
    if ((x > 0) && (x < len)) {// x < len since snprintf() adds a terminator
        bytesEncoded = x;      // but doesn't count it
    }

    return bytesEncoded;
}

/** Encode a wake-up reason data item: |,"d":{"rsn":"rtc"}|
 */
static int encodeDataWakeUpReason(char *pBuf, int len, DataWakeUpReason *pData)
{
    int bytesEncoded = -1;
    int x;

    // Attempt to snprintf() the string
    x = snprintf(pBuf, len, ",\"d\":{\"rsn\":\"%s\"}", gpWakeUpReason[pData->wakeUpReason]);
    if ((x > 0) && (x < len)) {// x < len since snprintf() adds a terminator
        bytesEncoded = x;      // but doesn't count it
    }

    return bytesEncoded;
}

/** Encode an energy source data item: |,"d":{"src":2}|
 */
static int encodeDataEnergySource(char *pBuf, int len, DataEnergySource *pData)
{
    int bytesEncoded = -1;
    int x;

    // Attempt to snprintf() the string
    x = snprintf(pBuf, len, ",\"d\":{\"src\":%u}", pData->x);
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
    x = snprintf(pBuf, len, ",\"d\":{\"stpd\":%u,\"wtpd\":%u,\"wpd\":%u,\"apd\":[",
                 pData->sleepTimePerDaySeconds, pData->wakeTimePerDaySeconds,
                 pData->wakeUpsPerDay);
    if ((x > 0) && (x < len)) {               // x < len since snprintf() adds a terminator
        ADVANCE_BUFFER(pBuf, len, x, total);  // but doesn't count it
        // Add the array of action counts per day
        for (unsigned int y = 0; keepGoing && (y < ARRAY_SIZE(pData->actionsPerDay)); y++) {
            x = snprintf(pBuf, len, "%u,", pData->actionsPerDay[y]);
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

/** Encode a log data item: |,"d":{"rec":[[235825,4,1],[235827,5,0]]}|
 */
static int encodeDataLog(char *pBuf, int len, DataLog *pData)
{
    int bytesEncoded = -1;
    bool keepGoing = true;
    int x;
    unsigned int y;
    int total = 0;

    // Attempt to snprintf() the prefix
    x = snprintf(pBuf, len, ",\"d\":{\"rec\":[");
    if ((x > 0) && (x < len)) {               // x < len since snprintf() adds a terminator
        ADVANCE_BUFFER(pBuf, len, x, total);  // but doesn't count it
        for (y = 0; keepGoing && (y < pData->numItems); y++) {
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
            } else {
                // Didn't go around the loop so just add a closing square bracket
                x = snprintf(pBuf, len, "]");
                if ((x > 0) && (x < len)) {               // x < len since snprintf() adds a terminator
                    ADVANCE_BUFFER(pBuf, len, x, total);  // but doesn't count it
                } else {
                    keepGoing = false;
                }
            }
            if (keepGoing) {
                // Add the closing brace
                x = snprintf(pBuf, len, "}");
                if ((x > 0) && (x < len)) {               // x < len since snprintf() adds a terminator
                    ADVANCE_BUFFER(pBuf, len, x, total);  // but doesn't count it
                    bytesEncoded = total;
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

// Prepare the data for coding, which simply means sort it.
void codecPrepareData()
{
    gpData = pDataSort();
}

// Encode queued data into a buffer.
// This function is veeeryyyy looooong.  Sorry about that, but there's
// no easy way to make it shorter without hiding things in even more macros,
// which I considered undesirable.
int codecEncodeData(const char *pNameString, char *pBuf, int len)
{
    int bytesEncoded = 0;
    int bytesEncodedThisDataItem = 0;
    int itemsEncoded = 0;
    char *pBufLast;
    int lenLast;
    int braceDepthLast;
    unsigned int energyCostUWH;
    int x;
    bool needComma = false;

    // Get the next data item
    if (gpData != NULL) {
        // If there is data to send, code the index and name fields
        x = encodeIndexAndName(pBuf, len, pNameString);
        if (x > 0) {
            ADVANCE_BUFFER(pBuf, len, x, bytesEncoded);
            // If there was room for that, and there
            // is enough room to close the JSON braces,
            // sort the data and see if there is anything to encode
            if (len >= gBraceDepth) {
                // Committed to actually returning a report now so can
                // increment the report index
                gReportIndex++;
                // Code the report start
                x = encodeReportStart(pBuf, len);
                if (x > 0) {
                    ADVANCE_BUFFER(pBuf, len, x, bytesEncoded);
                    if (len >= gBraceDepth) {
                        // If there was enough room for that,
                        // and the closing braces, add the data items
                        while ((bytesEncodedThisDataItem >= 0) && (gpData  != NULL)) {
                            pBufLast = pBuf;
                            lenLast = len;
                            braceDepthLast = gBraceDepth;
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
                                if (gpData->pAction != NULL) {
                                    energyCostUWH = gpData->pAction->energyCostUWH;
                                }
                                switch (gpData->type) {
                                    case DATA_TYPE_CELLULAR:
                                        x = encodeDataHeader(pBuf, len, "cel", gpData->timeUTC,
                                                             energyCostUWH);
                                        if (x > 0) {
                                            ADVANCE_BUFFER(pBuf, len, x, bytesEncodedThisDataItem);
                                            if (len >= gBraceDepth) {
                                                x = encodeDataCellular(pBuf, len, &gpData->contents.cellular);
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
                                        x = encodeDataHeader(pBuf, len, "hum", gpData->timeUTC,
                                                             energyCostUWH);
                                        if (x > 0) {
                                            ADVANCE_BUFFER(pBuf, len, x, bytesEncodedThisDataItem);
                                            if (len >= gBraceDepth) {
                                                x = encodeDataHumidity(pBuf, len, &gpData->contents.humidity);
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
                                        x = encodeDataHeader(pBuf, len, "pre", gpData->timeUTC,
                                                             energyCostUWH);
                                        if (x > 0) {
                                            ADVANCE_BUFFER(pBuf, len, x, bytesEncodedThisDataItem);
                                            if (len >= gBraceDepth) {
                                                x = encodeDataAtmosphericPressure(pBuf, len,
                                                                                  &gpData->contents.atmosphericPressure);
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
                                        x = encodeDataHeader(pBuf, len, "tmp", gpData->timeUTC,
                                                             energyCostUWH);
                                        if (x > 0) {
                                            ADVANCE_BUFFER(pBuf, len, x, bytesEncodedThisDataItem);
                                            if (len >= gBraceDepth) {
                                                x = encodeDataTemperature(pBuf, len, &gpData->contents.temperature);
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
                                        x = encodeDataHeader(pBuf, len, "lgt", gpData->timeUTC,
                                                             energyCostUWH);
                                        if (x > 0) {
                                            ADVANCE_BUFFER(pBuf, len, x, bytesEncodedThisDataItem);
                                            if (len >= gBraceDepth) {
                                                x = encodeDataLight(pBuf, len, &gpData->contents.light);
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
                                        x = encodeDataHeader(pBuf, len, "ori", gpData->timeUTC,
                                                             energyCostUWH);
                                        if (x > 0) {
                                            ADVANCE_BUFFER(pBuf, len, x, bytesEncodedThisDataItem);
                                            if (len >= gBraceDepth) {
                                                x = encodeDataOrientation(pBuf, len, &gpData->contents.orientation);
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
                                        x = encodeDataHeader(pBuf, len, "pos", gpData->timeUTC,
                                                             energyCostUWH);
                                        if (x > 0) {
                                            ADVANCE_BUFFER(pBuf, len, x, bytesEncodedThisDataItem);
                                            if (len >= gBraceDepth) {
                                                x = encodeDataPosition(pBuf, len, &gpData->contents.position);
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
                                        x = encodeDataHeader(pBuf, len, "mag", gpData->timeUTC,
                                                             energyCostUWH);
                                        if (x > 0) {
                                            ADVANCE_BUFFER(pBuf, len, x, bytesEncodedThisDataItem);
                                            if (len >= gBraceDepth) {
                                                x = encodeDataMagnetic(pBuf, len, &gpData->contents.magnetic);
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
                                        x = encodeDataHeader(pBuf, len, "ble", gpData->timeUTC,
                                                             energyCostUWH);
                                        if (x > 0) {
                                            ADVANCE_BUFFER(pBuf, len, x, bytesEncodedThisDataItem);
                                            if (len >= gBraceDepth) {
                                                x = encodeDataBle(pBuf, len, &gpData->contents.ble);
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
                                        x = encodeDataHeader(pBuf, len, "wkp", gpData->timeUTC,
                                                             energyCostUWH);
                                        if (x > 0) {
                                            ADVANCE_BUFFER(pBuf, len, x, bytesEncodedThisDataItem);
                                            if (len >= gBraceDepth) {
                                                x = encodeDataWakeUpReason(pBuf, len, &gpData->contents.wakeUpReason);
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
                                        x = encodeDataHeader(pBuf, len, "nrg", gpData->timeUTC,
                                                             energyCostUWH);
                                        if (x > 0) {
                                            ADVANCE_BUFFER(pBuf, len, x, bytesEncodedThisDataItem);
                                            if (len >= gBraceDepth) {
                                                x = encodeDataEnergySource(pBuf, len, &gpData->contents.energySource);
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
                                        x = encodeDataHeader(pBuf, len, "stt", gpData->timeUTC,
                                                             energyCostUWH);
                                        if (x > 0) {
                                            ADVANCE_BUFFER(pBuf, len, x, bytesEncodedThisDataItem);
                                            if (len >= gBraceDepth) {
                                                x = encodeDataStatistics(pBuf, len, &gpData->contents.statistics);
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
                                        x = encodeDataHeader(pBuf, len, "log", gpData->timeUTC,
                                                             energyCostUWH);
                                        if (x > 0) {
                                            ADVANCE_BUFFER(pBuf, len, x, bytesEncodedThisDataItem);
                                            if (len >= gBraceDepth) {
                                                x = encodeDataLog(pBuf, len, &gpData->contents.log);
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
                                        itemsEncoded++;
                                        bytesEncoded += bytesEncodedThisDataItem;
                                        bytesEncodedThisDataItem = 0;
                                        if ((gpData->flags & DATA_FLAG_REQUIRES_ACK) == 0) {
                                            dataFree(&gpData);
                                        }
                                        gpData = pDataNext();
                                        needComma = true;
                                    }
                                }
                            }
                        } // while ((bytesEncodedThisDataItem >= 0) && (gpData != NULL))
                        if (bytesEncodedThisDataItem < 0) {
                            // If we were in the middle of encoding something
                            // that didn't fit then set the buffer and the brace
                            // depth back to the last good point
                            len = lenLast;
                            pBuf = pBufLast;
                            gBraceDepth = braceDepthLast;
                        }
                    } else {  // if (len >= gBraceDepth)
                        // Not enough room to encode a report plus
                        // closing braces so rewind the buffer
                        REWIND_BUFFER(pBuf, len, x, bytesEncoded);
                    }  // if (len >= gBraceDepth)
                } // if ((x = encodeReportStart(pBuf, len)) > 0)
                // Add the correct number of closing braces
                while (gBraceDepth > 0) {
                    x = encodeCharacter(pBuf, len, '}');
                    if (x > 0) {
                        ADVANCE_BUFFER(pBuf, len, x, bytesEncoded);
                    }
                }
            } else { // if (len >= gBraceDepth)
                // Not even enough room to encode the initial
                // index plus its closing brace so rewind the buffer
                REWIND_BUFFER(pBuf, len, x, bytesEncoded);
            } // if (len >= gBraceDepth)
        } // if ((x = encodeIndex(pBuf, len)) > 0)
    } // if (gpData != NULL)

    // If no items were encoded and yet there were
    // items to encode then the buffer we were given
    // was not big enough.  Let the caller know this
    // be returning -1 (even though the output will,
    // in fact, be valid JSON).
    if ((itemsEncoded == 0) && (gpData != NULL)) {
        bytesEncoded = -1;
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
    // of removing everything that has been
    // reported.
    pStop = pDataNext();
    pData = pDataFirst();
    while (pData != pStop) {
        MBED_ASSERT(pData->flags & DATA_FLAG_REQUIRES_ACK);
        dataFree(&pData);
        pData = pDataNext();
    }
}

// End of file
