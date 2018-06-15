#include "greentea-client/test_env.h"
#include "unity.h"
#include "utest.h"
#include "mbed_trace.h"
#include "mbed.h"
#include "eh_data.h"
#include "eh_codec.h"
#define TRACE_GROUP "CODEC"

using namespace utest::v1;

// These are tests for the eh_codec module.
// I tried using Picojson to validate the JSON
// structures that are output but, unfortunately,
// it threw hard faults all the time and so, instead
// the JSON output must be visually inspected.
// Please set:
//
// "mbed-trace.enable": true
//
// ...in the target_overrides section of mbed_app.json
// and take a look at the output strings (delimited with
// vertical bars), maybe paste them into jsonlint.com.
//
// ----------------------------------------------------------------
// COMPILE-TIME MACROS
// ----------------------------------------------------------------

// ----------------------------------------------------------------
// PRIVATE VARIABLES
// ----------------------------------------------------------------

// Lock for debug prints
static Mutex gMtx;

// Storage for data contents
static DataContents gContents;

// ----------------------------------------------------------------
// PRIVATE FUNCTIONS
// ----------------------------------------------------------------

#ifdef MBED_CONF_MBED_TRACE_ENABLE
// Locks for debug prints
static void lock()
{
    gMtx.lock();
}

static void unlock()
{
    gMtx.unlock();
}
#endif

// Create a data item with valid contents
void createDataItem(DataContents *pContents, DataType type, char flags, Action *pAction)
{
    // For most things just fill the data contents with 0xFF
    // as that shows whether negative (or not) numbers are represented
    // properly
    memset (pContents, 0xFF, sizeof (*pContents));
    if (type == DATA_TYPE_BLE) {
        // For BLE, the name has to be a valid string or it won't print properly
        strcpy(gContents.ble.name, "BLE-THING");
    } else if (type == DATA_TYPE_WAKE_UP_REASON) {
        // Wake-up reason needs to be a valid one
        gContents.wakeUpReason.wakeUpReason = WAKE_UP_ORIENTATION;
    }
    TEST_ASSERT(pDataAlloc(pAction, type, flags, pContents) != NULL);
}

// ----------------------------------------------------------------
// TESTS
// ----------------------------------------------------------------

// Test print at least one of each data item
// Note: this really needs visual inspection
void test_print_all_data_items() {
    mbed_stats_heap_t statsHeapBefore;
    mbed_stats_heap_t statsHeapAfter;
    Action action;
    char *pBuf;
    int x = 0;
    int y = 0;
    int mallocSize = CODEC_ENCODE_BUFFER_MIN_SIZE;

    tr_debug("Print something out as tr_debug seems to allocate from the heap when first called.\n");

    // Capture the heap stats before we start
    mbed_stats_heap_get(&statsHeapBefore);
    tr_debug("%d byte(s) of heap used at the outset.", (int) statsHeapBefore.current_size);

    // Malloc a buffer
    pBuf = (char *) malloc(mallocSize);
    TEST_ASSERT (pBuf != NULL);

    // Encode an empty data queue
    tr_debug("Encoded empty data queue:\n");
    TEST_ASSERT(codecEncodeData(pBuf, mallocSize) == 0);

    
    // Fill up the data queue with one of each thing
    action.energyCostUWH = 0xFFFFFFFF;
    for (x = DATA_TYPE_NULL + 1; x < MAX_NUM_DATA_TYPES; x++) {
        createDataItem(&gContents, (DataType) x, 0, &action);
    }

    // Encode the queue
    tr_debug("Encoded full data queue:\n");
    codecPrepareData();
    while ((x = codecEncodeData(pBuf, mallocSize)) > 0) {
        tr_debug("%d (%d byte(s)): |%.*s|\n", y + 1, x, x, pBuf);
        // Test for JSON compliance
        y++;
    }

    free(pBuf);
    // Capture the heap stats once more
    mbed_stats_heap_get(&statsHeapAfter);
    tr_debug("%d byte(s) of heap used at the end.", (int) statsHeapAfter.current_size);

    // The heap used should be the same as at the start
    TEST_ASSERT(statsHeapBefore.current_size == statsHeapAfter.current_size);
}

// Test that acknowledged items are kept
void test_ack() {
    mbed_stats_heap_t statsHeapBefore;
    mbed_stats_heap_t statsHeapAfter;
    Action action;
    char *pBuf;
    int mallocSize = CODEC_ENCODE_BUFFER_MIN_SIZE;
    int bytesEncoded = 0;
    int x = 0;
    int y = 0;
    int z = 0;

    tr_debug("Print something out as tr_debug seems to allocate from the heap when first called.\n");

    // Capture the heap stats before we start
    mbed_stats_heap_get(&statsHeapBefore);
    tr_debug("%d byte(s) of heap used at the outset.", (int) statsHeapBefore.current_size);

    // Malloc a buffer
    pBuf = (char *) malloc(mallocSize);
    TEST_ASSERT (pBuf != NULL);

    // Fill up the data queue with one of each thing where each requires an ack
    action.energyCostUWH = 0xFFFFFFFF;
    for (x = DATA_TYPE_NULL + 1; x < MAX_NUM_DATA_TYPES; x++) {
        createDataItem(&gContents, (DataType) x, DATA_FLAG_REQUIRES_ACK, &action);
    }

    // Encode the queue but don't ack any of it
    tr_debug("One of each data type encoded:\n");
    codecPrepareData();
    while ((x = codecEncodeData(pBuf, mallocSize)) > 0) {
        tr_debug("%d (%d byte(s)): |%.*s|\n", y + 1, x, x, pBuf);
        // Test for JSON compliance
        bytesEncoded += x;
        y++;
    }
    tr_debug("Total bytes encoded: %d\n", bytesEncoded);

    // Now encode the queue again and the result should be the same
    tr_debug("The same data list encoded again:\n");
    codecPrepareData();
    while ((x = codecEncodeData(pBuf, mallocSize)) > 0) {
        tr_debug("%d (%d byte(s)): |%.*s|\n", y + 1, x, x, pBuf);
        // Test for JSON compliance
        z += x;
        y++;
    }
    tr_debug("Total bytes encoded: %d\n", z);
    TEST_ASSERT(z == bytesEncoded)

    // Now release the data
    codecAckData();

    free(pBuf);
    // Capture the heap stats once more
    mbed_stats_heap_get(&statsHeapAfter);
    tr_debug("%d byte(s) of heap used at the end.", (int) statsHeapAfter.current_size);

    // The heap used should be the same as at the start
    TEST_ASSERT(statsHeapBefore.current_size == statsHeapAfter.current_size);
}

// Print random contents into random buffer lengths
// Note: this really needs visual inspection
void test_rand() {
    mbed_stats_heap_t statsHeapBefore;
    mbed_stats_heap_t statsHeapAfter;
    Action action;
    char *pBuf;
    int x = 0;
    int y = 0;
    int z = 0;
    int mallocSize = CODEC_ENCODE_BUFFER_MIN_SIZE;
    int encodeSize;

    tr_debug("Print something out as tr_debug seems to allocate from the heap when first called.\n");

    // Capture the heap stats before we start
    mbed_stats_heap_get(&statsHeapBefore);
    tr_debug("%d byte(s) of heap used at the outset.", (int) statsHeapBefore.current_size);

    // Malloc a buffer
    pBuf = (char *) malloc(mallocSize);
    TEST_ASSERT (pBuf != NULL);

    // Do random stuff 10 times
    for (z = 0; z < 10; z++) {
        // Fill up the data queue with random types, randomly requiring acks
        for (x = 0; x < 50; x++) {
            y = (rand() % (MAX_NUM_DATA_TYPES - 1)) + 1;
            createDataItem(&gContents, (DataType) y, (rand() & 1) ? DATA_FLAG_REQUIRES_ACK : 0, &action);
        }

        // Encode the queue
        encodeSize = rand() % (mallocSize - 1);
        tr_debug("Encoded random data queue %d into buffer %d byte(s) big:\n", z + 1, encodeSize);
        y = 0;
        codecPrepareData();
        while ((x = codecEncodeData(pBuf, encodeSize)) > 0) {
            tr_debug("%d (%d byte(s)): |%.*s|\n", y + 1, x, x, pBuf);
            y++;
        }
        codecAckData();
    }

    free(pBuf);
    // Capture the heap stats once more
    mbed_stats_heap_get(&statsHeapAfter);
    tr_debug("%d byte(s) of heap used at the end.", (int) statsHeapAfter.current_size);

    // The heap used should be the same as at the start
    TEST_ASSERT(statsHeapBefore.current_size == statsHeapAfter.current_size);
}

// ----------------------------------------------------------------
// TEST ENVIRONMENT
// ----------------------------------------------------------------

// Setup the test environment
utest::v1::status_t test_setup(const size_t number_of_cases) {
    // Setup Greentea with a timeout
    GREENTEA_SETUP(120, "default_auto");
    return verbose_test_setup_handler(number_of_cases);
}

// Test cases
Case cases[] = {
    Case("Print all data items", test_print_all_data_items),
    Case("Ack", test_ack),
    Case("Random buffer lengths", test_rand)
};

Specification specification(test_setup, cases);

// ----------------------------------------------------------------
// MAIN
// ----------------------------------------------------------------

int main()
{

#ifdef MBED_CONF_MBED_TRACE_ENABLE
    mbed_trace_init();

    mbed_trace_mutex_wait_function_set(lock);
    mbed_trace_mutex_release_function_set(unlock);
#endif

    // Run tests
    return !Harness::run(specification);
}

// End Of File
