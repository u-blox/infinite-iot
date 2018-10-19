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

#include <stdio.h> // For sscanf()
#include <eh_data.h>
#include <eh_codec.h>
#include <eh_utilities.h> // For ARRAY_SIZE

/**************************************************************************
 * MANIFEST CONSTANTS
 *************************************************************************/

// Advance the buffer pointer and decrement the length based on
// x and add the number of bytes encoded to t
#define ADVANCE_BUFFER(pBuf, len, x, t) {(len) -= x; (pBuf) += x; (t) += x;}

// Rewind the buffer pointer and increment the length based on
// x and remove the number of bytes encoded fro t
#define REWIND_BUFFER(pBuf, len, x, t) {(len) += x; (pBuf) -= x; (t) -= x;}

/**************************************************************************
 * LOCAL VARIABLES
 *************************************************************************/

/** The point to start in the data list.
 */
static Data *gpData = NULL;

/** The report index.
 */
static int gReportIndex = 0;

/** The last used index.
 */
static int gLastUsedReportIndex = 0;

/** The bracket depth we are at in an encoded message.
 */
static int gBracketDepth = 0;

/** An array to track the type of closing brace to apply.
 */
static char gClosingBracket[10];

/** The possible wake-up reasons as text.
 */
static const char *gpWakeUpReason[] = {"PWR", "PIN", "WDG", "SOF", "RTC", "ACC", "MAG"};

/** The strings that form the name part of each data item when encoded.
 * Must be in the same order as DataType.
 */
static const char *gpDataName[] = {"",    /* DATA_TYPE_NULL */
                                   "cel", /* DATA_TYPE_CELLULAR */
                                   "hum", /* DATA_TYPE_HUMIDITY */
                                   "pre", /* DATA_TYPE_ATMOSPHERIC_PRESSURE */
                                   "tmp", /* DATA_TYPE_TEMPERATURE */
                                   "lgt", /* DATA_TYPE_LIGHT */
                                   "acc", /* DATA_TYPE_ACCELEROMETER */
                                   "pos", /* DATA_TYPE_POSITION */
                                   "mag", /* DATA_TYPE_MAGNETIC */
                                   "ble", /* DATA_TYPE_BLE */
                                   "wkp", /* DATA_TYPE_WAKE_UP_REASON */
                                   "nrg", /* DATA_TYPE_ENERGY_SOURCE */
                                   "stt", /* DATA_TYPE_STATISTICS */
                                   "log"  /* DATA_TYPE_LOG */};

/**************************************************************************
 * STATIC FUNCTIONS
 *************************************************************************/

/** Encode the index, name and ack part of a report, i.e.: |{"v":x,"n":"xxx","i":xxx,"a":x|
 * IMPORTANT: if you make a change here then you very likely need to also change
 * recodeAck(), which needs to be able to scanf() the report header.
 */
static int encodeHeader(char *pBuf, int len, const char *pNameString, bool ack)
{
    int bytesEncoded = -1;
    int x;

    // Attempt to snprintf() the string
    x = snprintf(pBuf, len, "{\"v\":%u,\"n\":\"%s\",\"i\":%u,\"a\":%c",
                 CODEC_PROTOCOL_VERSION, pNameString, gReportIndex, ack ? '1' : '0');
    if ((x > 0) && (x < len)) {// x < len since snprintf() adds a terminator
        bytesEncoded = x;      // but doesn't count it
        gClosingBracket[gBracketDepth] = '}';
        gBracketDepth++;
        MBED_ASSERT(gBracketDepth < (int) sizeof(gClosingBracket));
    }

    return bytesEncoded;
}

/** Re-encode the ack part of a report, header i.e.: |{..."a":x| by
 * finding it in the buffer and re-writing x.
 */
static void recodeAck(char *pBuf, bool ack)
{
    int x = 0;

    // Find the "a":x bit in the header
    sscanf(pBuf, "{\"v\":%*d,\"n\":\"%*[^\"]\",\"i\":%*u,\"a\":%n", &x);
    if (x > 0) {
        *(pBuf + x) = ack ? '1' : '0';
    }
}

/** Encode the report start, i.e.: |,"r":{|
 */
static int encodeReportStart(char *pBuf, int len)
{
    int bytesEncoded = -1;
    int x;

    // Attempt to snprintf() the string
    x = snprintf(pBuf, len, ",\"r\":[");
    if ((x > 0) && (x < len)) {// x < len since snprintf() adds a terminator
        bytesEncoded = x;      // but doesn't count it
        gClosingBracket[gBracketDepth] = ']';
        gBracketDepth++;
        MBED_ASSERT(gBracketDepth < (int) sizeof(gClosingBracket));
    }

    return bytesEncoded;
}

/** Encode the opening fields of a data item, e.g.: |"pos":{"t":xxxxxxx,"nWh":xxx|
 */
static int encodeDataHeader(char *pBuf, int len, const char *pPrefix, time_t timeUTC,
                            unsigned long long int energyCostNWH)
{
    int bytesEncoded = -1;
    int x;

    // Attempt to snprintf() the string
    x = snprintf(pBuf, len, "\"%s\":{\"t\":%d,\"nWh\":%llu", pPrefix, (int) timeUTC, energyCostNWH);
    if ((x > 0) && (x < len)) {// x < len since snprintf() adds a terminator
        bytesEncoded = x;      // but doesn't count it
        gClosingBracket[gBracketDepth] = '}';
        gBracketDepth++;
        MBED_ASSERT(gBracketDepth < (int) sizeof(gClosingBracket));
    }

    return bytesEncoded;
}

/** Encode a cellular data item: |,"d":{"rsrpdbm":-70,"rssidbm":-75,"rsrqDb":5,"snrdb":-5,"ecl":1,"cid":155,"tpwdbm":21,"ch":12412}|
 */
static int encodeDataCellular(char *pBuf, int len, DataCellular *pData)
{
    int bytesEncoded = -1;
    int x;

    // Attempt to snprintf() the string
    x = snprintf(pBuf, len, ",\"d\":{\"rsrpdbm\":%d,\"rssidbm\":%d,\"rsrqdb\":%d,"
                 "\"snrdb\":%d,\"ecl\":%u,\"cid\":%u,\"tpwdbm\":%d,\"ch\":%u}",
                 pData->rsrpDbm, pData->rssiDbm, pData->rsrqDb, pData->snrDb,
                 pData->ecl, pData->cellId, pData->transmitPowerDbm, pData->earfcn);
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

/** Encode an accelerometer data item: |,"d":{"xgx1000":5,"ygx1000":-1,"zgx1000":0}|
 */
static int encodeDataAcceleration(char *pBuf, int len, DataAcceleration *pData)
{
    int bytesEncoded = -1;
    int x;

    // Attempt to snprintf() the string
    x = snprintf(pBuf, len, ",\"d\":{\"xgx1000\":%d,\"ygx1000\":%d,\"zgx1000\":%d}", pData->xGX1000,
                 pData->yGX1000, pData->zGX1000);
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
    x = snprintf(pBuf, len, ",\"d\":{\"rsn\":\"%s\"}", gpWakeUpReason[pData->reason]);
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
 *                                  "ca":65,"cs":60,"cbt":352352,"cbr":252,"poa":40","pos":5,"svs":4}|
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
            x = snprintf(pBuf, len, ",\"epd\":%llu,\"ca\":%u,\"cs\":%u,"
                         "\"cbt\":%u,\"cbr\":%u,\"poa\":%u,\"pos\":%u,\"svs\":%u}",
                         pData->energyPerDayNWH, pData->cellularConnectionAttemptsSinceReset,
                         pData->cellularConnectionSuccessSinceReset,
                         pData->cellularBytesTransmittedSinceReset,
                         pData->cellularBytesReceivedSinceReset,
                         pData->positionAttemptsSinceReset,
                         pData->positionSuccessSinceReset,
                         pData->positionLastNumSvVisible);
            if ((x > 0) && (x < len)) {   // x < len since snprintf() adds a terminator
                bytesEncoded = x + total; // but doesn't count it
            }
        }
    }

    return bytesEncoded;
}

/** Encode a log data item: |,"d":{"v":"0.0","i":0,"rec":[[235825,4,1],[235827,5,0]]}|
 * The integer digit of "v" is the logApplicationVersion, the fractional digit is the
 * logClientVersion.  It is coded as a string, rather than a float, so that the far
 * end can pluck out the two values easily.
 */
static int encodeDataLog(char *pBuf, int len, DataLog *pData)
{
    int bytesEncoded = -1;
    bool keepGoing = true;
    int x;
    unsigned int y;
    int total = 0;

    // Attempt to snprintf() the prefix
    x = snprintf(pBuf, len, ",\"d\":{\"v\":\"%u.%u\",\"i\":%u,\"rec\":[",
                 pData->logApplicationVersion, pData->logClientVersion,
                 pData->index);
    if ((x > 0) && (x < len)) {               // x < len since snprintf() adds a terminator
        ADVANCE_BUFFER(pBuf, len, x, total);  // but doesn't count it
        for (y = 0; keepGoing && (y < pData->numItems); y++) {
            x = snprintf(pBuf, len, "[%u,%u,%u],", pData->log[y].timestamp,
                         pData->log[y].event, pData->log[y].parameter);
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

/** Encode a single character, incrementing or decrementing the
 * bracket count.
 */
static int encodeCharacter(char *pBuf, int len, char character)
{
    int bytesEncoded = -1;

    if (len > 0) {
        *pBuf = character;
        bytesEncoded = 1;
        if (((character == '}') || (character == ']') || (character == ')')) && (gBracketDepth > 0)) {
            gBracketDepth--;
        } else {
            switch (character) {
                case '{':
                    gClosingBracket[gBracketDepth] = '}';
                    gBracketDepth++;
                    MBED_ASSERT(gBracketDepth < (int) sizeof(gClosingBracket));
                break;
                case '[':
                    gClosingBracket[gBracketDepth] = ']';
                    gBracketDepth++;
                    MBED_ASSERT(gBracketDepth < (int) sizeof(gClosingBracket));
                break;
                case '(':
                    gClosingBracket[gBracketDepth] = ')';
                    gBracketDepth++;
                    MBED_ASSERT(gBracketDepth < (int) sizeof(gClosingBracket));
                break;
                default:
                break;
            }
        }
    }

    return bytesEncoded;
}

// Encode a data item
static int encodeDataItem(char *pBuf, int len, DataType dataType)
{
    int x;
    unsigned long long int energyCostNWH = 0;
    int bytesEncoded = 0;

    if (gpData->pAction != NULL) {
        energyCostNWH = gpData->pAction->energyCostNWH;
    }

    x = encodeDataHeader(pBuf, len, gpDataName[dataType], gpData->timeUTC,
                         energyCostNWH);
    if (x > 0) {
        ADVANCE_BUFFER(pBuf, len, x, bytesEncoded);
        if (len >= gBracketDepth) {
            switch (gpData->type) {
                case DATA_TYPE_CELLULAR:
                    x = encodeDataCellular(pBuf, len, &gpData->contents.cellular);
                break;
                case DATA_TYPE_HUMIDITY:
                    x = encodeDataHumidity(pBuf, len, &gpData->contents.humidity);
                break;
                case DATA_TYPE_ATMOSPHERIC_PRESSURE:
                    x = encodeDataAtmosphericPressure(pBuf, len,
                                                      &gpData->contents.atmosphericPressure);
                break;
                case DATA_TYPE_TEMPERATURE:
                    x = encodeDataTemperature(pBuf, len, &gpData->contents.temperature);
                break;
                case DATA_TYPE_LIGHT:
                    x = encodeDataLight(pBuf, len, &gpData->contents.light);
                break;
                case DATA_TYPE_ACCELERATION:
                    x = encodeDataAcceleration(pBuf, len, &gpData->contents.acceleration);
                break;
                case DATA_TYPE_POSITION:
                    x = encodeDataPosition(pBuf, len, &gpData->contents.position);
                break;
                case DATA_TYPE_MAGNETIC:
                    x = encodeDataMagnetic(pBuf, len, &gpData->contents.magnetic);
                break;
                case DATA_TYPE_BLE:
                    x = encodeDataBle(pBuf, len, &gpData->contents.ble);
                break;
                case DATA_TYPE_WAKE_UP_REASON:
                    x = encodeDataWakeUpReason(pBuf, len, &gpData->contents.wakeUpReason);
                break;
                case DATA_TYPE_ENERGY_SOURCE:
                    x = encodeDataEnergySource(pBuf, len, &gpData->contents.energySource);
                break;
                case DATA_TYPE_STATISTICS:
                    x = encodeDataStatistics(pBuf, len, &gpData->contents.statistics);
                break;
                case DATA_TYPE_LOG:
                    x = encodeDataLog(pBuf, len, &gpData->contents.log);
                break;
                default:
                    MBED_ASSERT(false);
                break;
            }
            if (x > 0) {
                ADVANCE_BUFFER(pBuf, len, x, bytesEncoded);
                if (len >= gBracketDepth) {
                    // Encode the closing brace
                    x = encodeCharacter(pBuf, len, gClosingBracket[gBracketDepth - 1]);
                    if (x > 0) {
                        ADVANCE_BUFFER(pBuf, len, x, bytesEncoded);
                        if (len < gBracketDepth) {
                            bytesEncoded = -1;
                        }
                    } else {
                        bytesEncoded = -1;
                    }
                } else {
                    bytesEncoded= -1;
                }
            } else {
                bytesEncoded= -1;
            }
        } else {
            bytesEncoded= -1;
        }
    } else {
        bytesEncoded= -1;
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
CodecFlagsAndSize codecEncodeData(const char *pNameString, char *pBuf, int len,
                                  bool needAck)
{
    int bytesEncoded = 0;
    unsigned int flags = 0;
    int bytesEncodedThisDataItem = 0;
    int itemsEncoded = 0;
    char *pBufStart = pBuf;
    char *pBufLast;
    int lenLast;
    int bracketDepthLast;
    int x;
    bool needComma = false;

    // Get the next data item
    if (gpData != NULL) {
        // If there is data to send, code the header with the
        // need for ack currently false (this may be changed later)
        x = encodeHeader(pBuf, len, pNameString, needAck);
        if (x > 0) {
            ADVANCE_BUFFER(pBuf, len, x, bytesEncoded);
            // If there was room for that, and there
            // is enough room to close the JSON braces,
            // sort the data and see if there is anything to encode
            if (len >= gBracketDepth) {
                // Committed to actually returning a report now so can
                // increment the report index, ensuring that it remains
                // a positive number
                gLastUsedReportIndex = gReportIndex;
                gReportIndex++;
                if (gReportIndex < 0) {
                    gReportIndex = 0;
                }
                // Code the report start
                x = encodeReportStart(pBuf, len);
                if (x > 0) {
                    ADVANCE_BUFFER(pBuf, len, x, bytesEncoded);
                    if (len >= gBracketDepth) {
                        // If there was enough room for that,
                        // and the closing braces, add the data items
                        while ((bytesEncodedThisDataItem >= 0) && (gpData != NULL)) {
                            pBufLast = pBuf;
                            lenLast = len;
                            bracketDepthLast = gBracketDepth;
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
                                    if (len < gBracketDepth) {
                                        bytesEncodedThisDataItem = -1;
                                    }
                                } else {
                                    bytesEncodedThisDataItem = -1;
                                }
                            }
                            // Put the opening brace on the report item
                            if (bytesEncodedThisDataItem >= 0) {
                                x = encodeCharacter(pBuf, len, '{');
                                if (x > 0) {
                                    ADVANCE_BUFFER(pBuf, len, x, bytesEncodedThisDataItem);
                                    if (len < gBracketDepth) {
                                        bytesEncodedThisDataItem = -1;
                                    }
                                } else {
                                    bytesEncodedThisDataItem = -1;
                                }
                            }
                            // Now encode the report item
                            if (bytesEncodedThisDataItem >= 0) {
                                x = encodeDataItem(pBuf, len, gpData->type);
                                // Put the closing brace on the item
                                if (x > 0) {
                                    ADVANCE_BUFFER(pBuf, len, x, bytesEncodedThisDataItem);
                                    if (len >= gBracketDepth) {
                                        x = encodeCharacter(pBuf, len, gClosingBracket[gBracketDepth - 1]);
                                        if (x > 0) {
                                            ADVANCE_BUFFER(pBuf, len, x, bytesEncodedThisDataItem);
                                            // If it was possible to encode a whole data item
                                            // then increment the total byte count and,
                                            // if the item doesn't require an ack, free it
                                            // immediately.
                                            if (len >= gBracketDepth) {
                                                itemsEncoded++;
                                                bytesEncoded += bytesEncodedThisDataItem;
                                                bytesEncodedThisDataItem = 0;
                                                if ((gpData->flags & DATA_FLAG_REQUIRES_ACK) != 0) {
                                                    needAck = true;
                                                } else {
                                                    dataFree(&gpData);
                                                }
                                                gpData = pDataNext();
                                                needComma = true;
                                            } else {
                                                bytesEncodedThisDataItem = -1;
                                            }
                                        } else {
                                            bytesEncodedThisDataItem = -1;
                                        }
                                    } else {
                                        bytesEncodedThisDataItem = -1;
                                    }
                                } else {
                                    bytesEncodedThisDataItem = -1;
                                }
                            }
                        } // while ((bytesEncodedThisDataItem >= 0) && (gpData != NULL))
                        if (bytesEncodedThisDataItem < 0) {
                            // If we were in the middle of encoding something
                            // that didn't fit then set the buffer and the brace
                            // depth back to the last good point
                            len = lenLast;
                            pBuf = pBufLast;
                            gBracketDepth = bracketDepthLast;
                        } else {
                            // Otherwise, put the closing brace of the report into place
                            // No need to check against lengths since this
                            // was already taken account of above by
                            // incrementing gBracketDepth in encodeReportStart()
                            x = encodeCharacter(pBuf, len, gClosingBracket[gBracketDepth - 1]);
                            MBED_ASSERT(x > 0);
                            ADVANCE_BUFFER(pBuf, len, x, bytesEncoded);
                        }
                    } else {  // if (len >= gBracketDepth)
                        // Not enough room to encode a report plus
                        // closing square brace so rewind the buffer
                        flags |= CODEC_FLAG_NOT_ENOUGH_ROOM_FOR_HEADER;
                        REWIND_BUFFER(pBuf, len, x, bytesEncoded);
                    }  // if (len >= gBracketDepth)
                } else {// if ((x = encodeReportStart(pBuf, len)) > 0)
                    flags |= CODEC_FLAG_NOT_ENOUGH_ROOM_FOR_HEADER;
                }
                // Add the correct number of closing braces
                while (gBracketDepth > 0) {
                    x = encodeCharacter(pBuf, len, gClosingBracket[gBracketDepth - 1]);
                    if (x > 0) {
                        ADVANCE_BUFFER(pBuf, len, x, bytesEncoded);
                    }
                }
            } else { // if (len >= gBracketDepth)
                // Not even enough room to encode the initial
                // index plus its closing brace so rewind the buffer
                flags |= CODEC_FLAG_NOT_ENOUGH_ROOM_FOR_HEADER;
                REWIND_BUFFER(pBuf, len, x, bytesEncoded);
            } // if (len >= gBracketDepth)
        } else {// if ((x = encodeIndex(pBuf, len)) > 0)
            flags |= CODEC_FLAG_NOT_ENOUGH_ROOM_FOR_HEADER;
        }
    } // if (gpData != NULL)

    // If we have encoded something that requires an ack, re-code
    // the header to say so
    if (needAck) {
        flags |= CODEC_FLAG_NEEDS_ACK;
        recodeAck(pBufStart, needAck);
    }

    // If no items were encoded and yet there were
    // items to encode then the buffer we were given
    // was not big enough.  Let the caller know this
    // by setting the appropriate flag.
    if ((itemsEncoded == 0) && (gpData != NULL)) {
        flags |= CODEC_FLAG_NOT_ENOUGH_ROOM_FOR_EVEN_ONE_DATA;
    }

    return (CodecFlagsAndSize) ((flags << 16) | (bytesEncoded & 0xFFFF));
}

// Remove acknowledged data.
void codecAckData()
{
    Data *pData;

    pData = pDataFirst();
    while (pData != gpData) {
        dataFree(&pData);
        pData = pDataNext();
    }
}

// Get the last index value that was encoded
int codecGetLastIndex()
{
    return gLastUsedReportIndex;
}

// Decode a buffer that is expected to contain an ack message.
CodecErrorOrIndex codecDecodeAck(char *pBuf, int len, const char *pNameString)
{
    int returnValue = CODEC_ERROR_BAD_PARAMETER;
    int i = 0;
    int x = 0;
    char name[CODEC_MAX_NAME_STRLEN + 1]; // +1 for terminator
    int nameStringLen = strlen(pNameString);
    int bufStringLen = strlen(pBuf);

    // sscanf() works on strings, so need to treat pBuf as a string
    if (len > (int) bufStringLen) {
        len = bufStringLen;
    }
    memset(name, 0, sizeof(name)); // Make name[] an empty string
    if (nameStringLen <= (int) sizeof(name) - 1) {
        returnValue = CODEC_ERROR_NOT_ACK_MSG;
        // Note hard coded 32 below, which must match CODEC_MAX_NAME_STRLEN
        // The need for hard-coding is a limitation of the standard sscanf()
        // library I'm afraid
        // Note: the whitespace is significant, it permits zero or more
        // whitespace in all of those places in the sequence
        // Note: need the %n on the end otherwise we might not notice if the
        // trailing brace is missing
        if ((sscanf(pBuf, " { \"n\" : \"%32[^\"]\" , \"i\" : %d }%n", name, &i, &x) == 2) &&
            (x > 0) && (len >= x)) {
            returnValue = CODEC_ERROR_NO_NAME_MATCH;
            // It's of the right form, but does the name match?
            if (((int) strlen(name) == nameStringLen) &&
                (strcmp(name, pNameString) == 0)) {
                returnValue = i;
            }
        }
    }

    return (CodecErrorOrIndex) returnValue;
}

// End of file
