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
#include <eh_utilities.h> // for MTX_LOCK()/MTX_UNLOCK(()
#include <eh_data.h>

/**************************************************************************
 * MANIFEST CONSTANTS
 *************************************************************************/

/**  Convert a size in bytes to a size in words, rounding up as necessary.
 */
#define TO_WORDS(bytes) (((bytes) / 4) + (((bytes) % 4) == 0 ? 0 : 1))

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
                                         sizeof(DataAcceleration), /* DATA_TYPE_ACCELERATION */
                                         sizeof(DataPosition), /* DATA_TYPE_POSITION */
                                         sizeof(DataMagnetic), /* DATA_TYPE_MAGNETIC */
                                         sizeof(DataBle), /* DATA_TYPE_BLE */
                                         sizeof(DataWakeUpReason), /* DATA_TYPE_WAKE_UP_REASON */
                                         sizeof(DataEnergySource), /* DATA_TYPE_ENERGY_SOURCE */
                                         sizeof(DataStatistics), /* DATA_TYPE_STATISTICS */
                                         sizeof(DataLog) /* DATA_TYPE_LOG */};

/** Pointer to the start of the data buffer.
 * NOTE: int rather than char to ensure worst-case alignment.
 */
static int *gpBuffer = NULL;

/** Pointer to the first full location in the data buffer.
 */
static int *gpBufferFirstFull = NULL;

/** Pointer to the next free location in the data buffer.
 */
static int *gpBufferNextEmpty = gpBuffer;

/** Pointer to the "next" pointer location on the previous
 * entry in the data buffe.
 */
static int *gpBufferPreviousNext = NULL;

/** The root of the data link list.
 */
static Data *gpDataList = NULL;

/** Mutex to protect the data lists.
 */
static Mutex gMtx;

/** A pointer to the next data item.
 */
static Data *gpNextData = NULL;

/** Keep track of the amount of RAM used for storing data.
 */
static unsigned int gDataSizeUsed = 0;

/**************************************************************************
 * STATIC FUNCTIONS
 *************************************************************************/

// A word about the memory allocation arrangements here:
// The original intention was that data items would be malloc()ed
// as necessary and tracked with a linked list.  However, it turns
// out that the small allocations lead to fragmention so that, for
// example, with 13 kbytes of heap free 4 kbytes (for the modem to
// be activated) were not available. So, as an alternative, this
// mallocater was introduced.  It relies on the fact that the data
// items are generally freed first-in first-out to keep it simple.

// Set *gpBufferPreviousNext and then move gpBufferNextEmpty
// and gpBufferPreviousNext on.
static void advanceMemoryPointers(int mallocSizeWords)
{
    if (gpBufferPreviousNext != NULL) {
        // Set the pointer at the end of the previous
        // entry to point to this one
        *gpBufferPreviousNext = (int) gpBufferNextEmpty;
#ifdef CHAPTER_AND_VERSE
        printf("*gpBufferPreviousNext (0x%08x) set to 0x%08x.\n",
                gpBufferPreviousNext, gpBufferNextEmpty);
#endif
    }
    gpBufferNextEmpty += mallocSizeWords;
    // Set pointer to next entry on this one to NULL
    gpBufferPreviousNext = gpBufferNextEmpty - 1;
    *gpBufferPreviousNext = (int) NULL;
    // Check if gpBufferNextEmpty was right on the edge and
    // so needs to be wrapped
    if (gpBufferNextEmpty >= gpBuffer + DATA_MAX_SIZE_WORDS) {
        gpBufferNextEmpty = gpBuffer;
    }
#ifdef CHAPTER_AND_VERSE
    printf("Advancing: gpBufferNextEmpty 0x%08x, gpBufferPreviousNext 0x%08x.\n",
            gpBufferNextEmpty, gpBufferPreviousNext);
#endif
}

// Allocate memory, or check if alloc'ing would work.
// IMPORTANT if allocNotCheck is false then do NOT use the returned
// pointer as it is simply a Boolean indicator, NULL or not NULL.
static Data *pMemoryAlloc(DataType type, bool allocNotCheck, unsigned int *pBytesUsed)
{
    Data *pData = NULL;
    int mallocSizeWords = TO_WORDS(offsetof(Data, contents) + gSizeOfContents[type]);

    if (gpBuffer != NULL) {
        // Add one to the malloc size since we use an int
        // at the end to point to the next block in order
        // to deal with the corner case of something not
        // fitting (so the blocks are not adjacent) at the
        // end of the buffer
        mallocSizeWords++;
#ifdef CHAPTER_AND_VERSE
        printf("Mallocing %d words (%d bytes).\n", mallocSizeWords, mallocSizeWords * 4);
#endif
        // If we're using an internal buffer, and there's room,
        // allocate from it
        if (gpBufferFirstFull == NULL) {
            gpBufferPreviousNext = NULL;
            // Empty case
#ifdef CHAPTER_AND_VERSE
            printf("In empty case.\n");
#endif
            if (gpBufferNextEmpty - gpBuffer + mallocSizeWords <= DATA_MAX_SIZE_WORDS) {
#ifdef CHAPTER_AND_VERSE

                printf("gpBufferNextEmpty (0x%08x) - gpBuffer (0x%08x) (==%d) + %d is less than or equal to %d).\n",
                        gpBufferNextEmpty, gpBuffer, gpBufferNextEmpty - gpBuffer, mallocSizeWords, DATA_MAX_SIZE_WORDS);
#endif
                pData = (Data *) gpBufferNextEmpty;
                if (allocNotCheck) {
                    advanceMemoryPointers(mallocSizeWords);
                    // We now have a "first full"
                    gpBufferFirstFull = (int *) pData;
#ifdef CHAPTER_AND_VERSE
                    printf("gpBufferFirstFull set to 0x%08x.\n", gpBufferFirstFull);
#endif
                }
            }
        } else {
            if (gpBufferNextEmpty > gpBufferFirstFull) {
#ifdef CHAPTER_AND_VERSE
                printf("In \"empty (E) is ahead of first full (F)\" case.\n");
#endif
                // Next empty (E) is ahead of first full (F)
                // |-----#####-------|
                //       F    E
                if (gpBufferNextEmpty - gpBuffer + mallocSizeWords <= DATA_MAX_SIZE_WORDS) {
#ifdef CHAPTER_AND_VERSE
                    printf("gpBufferNextEmpty (0x%08x) - gpBuffer (0x%08x) (==%d) + %d is less than or equal to %d.\n",
                            gpBufferNextEmpty, gpBuffer, gpBufferNextEmpty - gpBuffer, mallocSizeWords, DATA_MAX_SIZE_WORDS);
#endif
                    // Enough room before the end of the buffer to allocate
                    // the block so do so and move the "next empty" pointer on
                    // |-----#####xxx----|
                    //       F       E
                    pData = (Data *) gpBufferNextEmpty;
                    if (allocNotCheck) {
                        advanceMemoryPointers(mallocSizeWords);
                    }
                } else {
                    // Not enough room before the end of the buffer so see
                    // if there would be room if we wrapped around to the
                    // start again
#ifdef CHAPTER_AND_VERSE
                    printf("No room at end, see if we can wrap.\n");
#endif
                    if (gpBufferFirstFull == gpBuffer) {
#ifdef CHAPTER_AND_VERSE
                        printf("gpBufferFirstFull == gpBuffer(0x%08x).\n", gpBuffer);
#endif
                        // Nope, we would hit the "first full" pointer so we're full
                    } else {
                        if (gpBufferFirstFull - gpBuffer >= mallocSizeWords) {
#ifdef CHAPTER_AND_VERSE
                            printf("gpBufferFirstFull (0x%08x) - gpBuffer (0x%08x) (== %d) is greater than or equal to %d.\n",
                                    gpBufferFirstFull, gpBuffer, gpBufferFirstFull - gpBuffer, mallocSizeWords);
#endif
                            // Enough room at the start of the buffer so allocate
                            // and move the "next empty" pointer on to there
                            // |xxx--#####------|
                            //     E F
                            pData = (Data *) gpBuffer;
                            if (allocNotCheck) {
                                gpBufferNextEmpty = gpBuffer;
                                advanceMemoryPointers(mallocSizeWords);
                            }
                        } else {
                            // Nope, not enough room at the start of the buffer either
                        }
                    }
                }
            } else {
#ifdef CHAPTER_AND_VERSE
                printf("In \"empty is behind first full\" case.\n");
#endif
                // Next empty is behind first full
                // |###-------####--|
                //     E      F
                if (gpBufferNextEmpty < gpBufferFirstFull) {
                    if (gpBufferFirstFull - gpBufferNextEmpty >= mallocSizeWords) {
#ifdef CHAPTER_AND_VERSE
                        printf("gpBufferFirstFull (0x%08x) - gpBufferNextEmpty (0x%08x) (== %d) + %d is greater than or equal to %d.\n",
                                gpBufferFirstFull, gpBufferNextEmpty, gpBufferFirstFull - gpBuffer, mallocSizeWords, DATA_MAX_SIZE_WORDS);
#endif
                        // Enough room, so allocate and move the "next empty"
                        // pointer on
                        // |###xxx----####--|
                        //        E   F
                        pData = (Data *) gpBufferNextEmpty;
                        if (allocNotCheck) {
                            advanceMemoryPointers(mallocSizeWords);
                        }
                    }
                } else {
#ifdef CHAPTER_AND_VERSE
                    printf("gpBufferNextEmpty (0x%08x) is equal to gpBufferFirstFull (0x%08x).\n",
                            gpBufferNextEmpty, gpBufferFirstFull);
#endif
                    // Next empty and first full pointers must be
                    // equal, so no room left
                }
            }
        }
    } else {
        if (gDataSizeUsed + (mallocSizeWords * 4) <= DATA_MAX_SIZE_BYTES) {
            // No internal buffer, just call malloc()
            pData = (Data *) malloc(mallocSizeWords * 4);
            if (!allocNotCheck && (pData != NULL)) {
                free(pData);
            }
        }
    }

    if ((pData != NULL) && (pBytesUsed != NULL)) {
        *pBytesUsed = mallocSizeWords * 4;
    }

    return pData;
}

// Free memory.
static unsigned int memoryFree(Data *pData)
{
    unsigned int bytesFreed = 0;

    if (gpBuffer != NULL) {
        // We're using an internal buffer...
        if (pData != NULL) {
            // Mark the data item as freeable
            pData->flags |= DATA_FLAG_CAN_BE_FREED;
#ifdef CHAPTER_AND_VERSE
            printf("Marking 0x%08x as freeable (%d bytes).\n", pData, TO_WORDS(offsetof(Data, contents) + gSizeOfContents[pData->type]) * 4);
#endif
            if ((int *) pData == gpBufferFirstFull) {
                // If this is the "first full" entry,
                // start freeing entries from this
                // point forward
                while ((pData != NULL) &&
                       (pData->flags & DATA_FLAG_CAN_BE_FREED)) {
                    // Move the "first full" pointer on to the
                    // one stored at the end of the current
                    // data block
#ifdef CHAPTER_AND_VERSE
                    printf("Freeing 0x%08x.\n", gpBufferFirstFull);
#endif
                    gpBufferFirstFull = (int *) *(gpBufferFirstFull +
                                                  (TO_WORDS(offsetof(Data, contents) +
                                                            gSizeOfContents[pData->type])));
#ifdef CHAPTER_AND_VERSE
                    printf("gpBufferFirstFull is now 0x%08x.\n", gpBufferFirstFull);
#endif
                    bytesFreed += TO_WORDS(offsetof(Data, contents) + gSizeOfContents[pData->type]) * 4;
                    // Set pData to the next block (may be NULL)
                    pData = (Data *) gpBufferFirstFull;
                }
            } else {
                // Can't free it now, it is marked for
                // freeing later
            }
        }
    } else {
        // No internal buffer, just call free()
        bytesFreed = TO_WORDS(offsetof(Data, contents) + gSizeOfContents[pData->type]) * 4;
        free(pData);
    }

    return bytesFreed;
}

// Condition function to return true if pNextData has a higher flag value
// than pData or, if the flags are the same, then return true if
// pNextData is newer (a higher number) than pData.  The flag data
// is shifted left by one to stop the DATA_FLAG_CAN_BE_FREED flag
// from having an effect.
static bool conditionFlags(Data *pData, Data *pNextData)
{
    return (((pNextData->flags >> 1) > (pData->flags >> 1)) ||
            (((pNextData->flags >> 1) == (pData->flags >> 1)) &&
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

// Initialise data memory
void dataInit(int *pBuffer)
{
    gpBuffer = pBuffer;
    gpBufferNextEmpty = gpBuffer;
}

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
        case DATA_TYPE_ACCELERATION:
            // For acceleration use the largest of the x, y, or z values
            difference = pData1->contents.acceleration.xGX1000 - pData2->contents.acceleration.xGX1000;
            x = pData1->contents.acceleration.yGX1000 - pData2->contents.acceleration.yGX1000;
            if (abs(x) > abs(difference)) {
                difference = x;
            }
            x = pData1->contents.acceleration.zGX1000 - pData2->contents.acceleration.zGX1000;
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
    Data **ppThis = NULL;
    Data *pPrevious;
    unsigned int x;

    MTX_LOCK(gMtx);

    MBED_ASSERT(type < MAX_NUM_DATA_TYPES);

    // Find the end of the data list
    pPrevious = NULL;
    ppThis = &(gpDataList);
    while (*ppThis != NULL) {
        pPrevious = *ppThis;
        ppThis = &((*ppThis)->pNext);
    }

    // Allocate room for the data
    *ppThis = pMemoryAlloc(type, true, &x);
    if (*ppThis != NULL) {
        gDataSizeUsed += x;
        // Copy in the data values
        (*ppThis)->timeUTC = time(NULL);
        (*ppThis)->type = type;
        (*ppThis)->flags = flags;
        (*ppThis)->pAction = pAction;
        if (pContents != NULL) {
            memcpy(&((*ppThis)->contents), pContents, gSizeOfContents[type]);
        }
        (*ppThis)->pPrevious = pPrevious;
        if ((*ppThis)->pPrevious != NULL) {
            (*ppThis)->pPrevious->pNext = (*ppThis);
        }
        (*ppThis)->pNext = NULL;

        if (pAction != NULL) {
            pAction->pData = *ppThis;
        }
    }

    MTX_UNLOCK(gMtx);

    return ppThis != NULL ? *ppThis : NULL;
}

// Remove a data item, free()ing memory.
void dataFree(Data **ppData)
{
    Data **ppThis;
    Data *pNext;
    unsigned int x;

    MTX_LOCK(gMtx);

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
            x = memoryFree(*ppData);
            if (gDataSizeUsed >= x) {
                gDataSizeUsed -= x;
            }
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

    MTX_UNLOCK(gMtx);
}

// Check if a request to allocated room for a given data type
// would succeed
bool dataAllocCheck(DataType type)
{
    Data *pData = pMemoryAlloc(type, false, NULL);

    return (pData != NULL);
}

// Return the number of data items.
int dataCount()
{
    Data **ppThis;
    int x;

    MTX_LOCK(gMtx);

    x = 0;
    ppThis = &(gpDataList);
    while (*ppThis != NULL) {
        x++;
        ppThis = &((*ppThis)->pNext);
    }

    MTX_UNLOCK(gMtx);

    return x;
}

// Count the number of data items of a given type.
int dataCountType(DataType type)
{
    Data **ppThis;
    int x;

    MTX_LOCK(gMtx);

    x = 0;
    ppThis = &(gpDataList);
    while (*ppThis != NULL) {
        if ((*ppThis)->type == type) {
            x++;
        }
        ppThis = &((*ppThis)->pNext);
    }

    MTX_UNLOCK(gMtx);

    return x;
}

// Sort the data list.
Data *pDataSort()
{
    MTX_LOCK(gMtx);

    sort(conditionFlags);
    gpNextData = gpDataList;

    MTX_UNLOCK(gMtx);

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
    MTX_LOCK(gMtx);

    if (gpNextData != NULL) {
        gpNextData = gpNextData->pNext;
    }

    MTX_UNLOCK(gMtx);

    return gpNextData;
}

// Get the number of bytes used in the data buffer.
unsigned int dataGetBytesUsed()
{
    return gDataSizeUsed;
}

// Get the number of bytes of data queued.
unsigned int dataGetBytesQueued()
{
    Data *pThis;
    unsigned int x;

    MTX_LOCK(gMtx);

    x = 0;
    pThis = gpDataList;
    while (pThis != NULL) {
        x += TO_WORDS(offsetof(Data, contents) + gSizeOfContents[pThis->type]) * 4;
        pThis = pThis->pNext;
    }

    MTX_UNLOCK(gMtx);

    return x;
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
