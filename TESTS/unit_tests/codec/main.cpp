#include "greentea-client/test_env.h"
#include "unity.h"
#include "utest.h"
#include "mbed_trace.h"
#include "mbed.h"
#include "eh_utilities.h" // for ARRAY_SIZE
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
// vertical bars), maybe paste them into jsonlint.com,
// though if you do that jsonlint will object to the multiple
// instances of any one data item which these tests cases
// generate (so just edit them to be different in the
// jsonlint.com text box).
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
static void createDataItem(DataContents *pContents, DataType type, char flags, Action *pAction)
{
    // For most things just fill the data contents with 0xFF
    // as that shows whether negative (or not) numbers are represented
    // properly
    memset (pContents, 0xFF, sizeof (*pContents));
    if (type == DATA_TYPE_BLE) {
        // For BLE, the name has to be a valid string or it won't print properly
        strcpy(gContents.ble.name, "BLE-THING");
    } else if (type == DATA_TYPE_LOG) {
        // Need a valid number of items
        pContents->log.numItems = ARRAY_SIZE(gContents.log.log);
    } else if (type == DATA_TYPE_WAKE_UP_REASON) {
        // Wake-up reason needs to be a valid one
        pContents->wakeUpReason.wakeUpReason = WAKE_UP_ORIENTATION;
    }
    TEST_ASSERT(pDataAlloc(pAction, type, flags, pContents) != NULL);
}

// Fill a buffer with random contents overlayed with the given string
static void fillBuf(char *pBuf, int len, char *pString)
{
    int x = strlen(pString);

    if (x > len) {
        x = len;
    }

    memcpy(pBuf, pString, len);
    for (int y = x; y < len; y++) {
        *(pBuf + y) = rand();
    }
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
    TEST_ASSERT(codecEncodeData("DevName", pBuf, mallocSize) == 0);

    
    // Fill up the data queue with one of each thing
    action.energyCostUWH = 0xFFFFFFFF;
    for (x = DATA_TYPE_NULL + 1; x < MAX_NUM_DATA_TYPES; x++) {
        createDataItem(&gContents, (DataType) x, 0, &action);
    }

    // Encode the queue
    tr_debug("Encoded full data queue:\n");
    codecPrepareData();
    while ((x = codecEncodeData("357520071700641", pBuf, mallocSize)) > 0) {
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

// Test that acknowledged items are kept until acknowledged
void test_ack_data() {
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
    while ((x = codecEncodeData("A name with spaces", pBuf, mallocSize)) > 0) {
        tr_debug("%d (%d byte(s)): |%.*s|\n", y + 1, x, x, pBuf);
        // Test for JSON compliance
        bytesEncoded += x;
        y++;
    }
    tr_debug("Total bytes encoded: %d\n", bytesEncoded);

    // Now encode the queue again and the result should be the same
    tr_debug("The same data list encoded again:\n");
    codecPrepareData();
    while ((x = codecEncodeData("A name with spaces", pBuf, mallocSize)) > 0) {
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
        while ((x = codecEncodeData("ThirtyTwoCharacterFieldAddedHere", pBuf, encodeSize)) > 0) {
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

// Test decoding
void test_decode() {
    mbed_stats_heap_t statsHeapBefore;
    mbed_stats_heap_t statsHeapAfter;
    char buf[128];

    tr_debug("Print something out as tr_debug seems to allocate from the heap when first called.\n");

    // Capture the heap stats before we start
    mbed_stats_heap_get(&statsHeapBefore);
    tr_debug("%d byte(s) of heap used at the outset.", (int) statsHeapBefore.current_size);

    // Create a buffer with a valid ack message and otherwise garbage
    fillBuf(buf, sizeof(buf), "{\"n\":\"357520071700641\",\"i\":4}");
    TEST_ASSERT(codecDecodeAck(buf, sizeof(buf), "357520071700641") == 4);
    // Make the name not match in the last character
    TEST_ASSERT(codecDecodeAck(buf, sizeof(buf), "357520071700640") == CODEC_DECODE_ERROR_NO_NAME_MATCH);
    // Make the name not match in the first character
    TEST_ASSERT(codecDecodeAck(buf, sizeof(buf), "257520071700641") == CODEC_DECODE_ERROR_NO_NAME_MATCH);
    // Make the name too small
    TEST_ASSERT(codecDecodeAck(buf, sizeof(buf), "35752007170064") == CODEC_DECODE_ERROR_NO_NAME_MATCH);
    // Make the name too large
    TEST_ASSERT(codecDecodeAck(buf, sizeof(buf), "3575200717006411") == CODEC_DECODE_ERROR_NO_NAME_MATCH);
    // Create a buffer with a maximum length name
    fillBuf(buf, sizeof(buf), "{\"n\":\"01234567890123456789012345678901\",\"i\":9}");
    TEST_ASSERT(codecDecodeAck(buf, sizeof(buf), "01234567890123456789012345678901") == 9);
    // Pass in a name that is too large
    TEST_ASSERT(codecDecodeAck(buf, sizeof(buf), "012345678901234567890123456789012") == CODEC_DECODE_ERROR_BAD_PARAMETER);
    // Try the maximum index number (0x7FFFFFFF)
    fillBuf(buf, sizeof(buf), "{\"n\":\"01234567890123456789012345678901\",\"i\":2147483647}");
    TEST_ASSERT(codecDecodeAck(buf, sizeof(buf), "01234567890123456789012345678901") == 2147483647);
    // Add spaces in all the possible places
    fillBuf(buf, sizeof(buf), " { \"n\" : \"01234567890123456789012345678901\" , \"i\" : 2147483647 }");
    TEST_ASSERT(codecDecodeAck(buf, sizeof(buf), "01234567890123456789012345678901") == 2147483647);
    // Make sure alpha is OK
    fillBuf(buf, sizeof(buf), "{\"n\":\"abcdefghijklmnopqrstuvwxyz\",\"i\":2147483647}");
    TEST_ASSERT(codecDecodeAck(buf, sizeof(buf), "abcdefghijklmnopqrstuvwxyz") == 2147483647);
    // Make sure alpha is OK
    fillBuf(buf, sizeof(buf), "{\"n\":\"ABCDEFGHIJKLMNOPQRSTUVWXYZ\",\"i\":2147483647}");
    TEST_ASSERT(codecDecodeAck(buf, sizeof(buf), "ABCDEFGHIJKLMNOPQRSTUVWXYZ") == 2147483647);
    // Make sure we ignore trailing stuff
    fillBuf(buf, sizeof(buf), "{\"n\":\"01234567890123456789012345678901\",\"i\":2147483647}x");
    TEST_ASSERT(codecDecodeAck(buf, sizeof(buf), "01234567890123456789012345678901") == 2147483647);
    // Try a few specific mis-formattings
    fillBuf(buf, sizeof(buf), "{\'n\':\'01234567890123456789012345678901\',\'i\':2147483647}");
    TEST_ASSERT(codecDecodeAck(buf, sizeof(buf), "01234567890123456789012345678901") == CODEC_DECODE_ERROR_NOT_ACK_MSG);
    fillBuf(buf, sizeof(buf), "{\"n\"\"01234567890123456789012345678901\",\"i\":2147483647}");
    TEST_ASSERT(codecDecodeAck(buf, sizeof(buf), "01234567890123456789012345678901") == CODEC_DECODE_ERROR_NOT_ACK_MSG);
    fillBuf(buf, sizeof(buf), "{\"n\":01234567890123456789012345678901,\"i\":2147483647}");
    TEST_ASSERT(codecDecodeAck(buf, sizeof(buf), "01234567890123456789012345678901") == CODEC_DECODE_ERROR_NOT_ACK_MSG);
    fillBuf(buf, sizeof(buf), "\"n\":\"01234567890123456789012345678901\",\"i\":2147483647}");
    TEST_ASSERT(codecDecodeAck(buf, sizeof(buf), "01234567890123456789012345678901") == CODEC_DECODE_ERROR_NOT_ACK_MSG);
    fillBuf(buf, sizeof(buf), "{\"n\":\"01234567890123456789012345678901\",\"i\":2147483647");
    TEST_ASSERT(codecDecodeAck(buf, sizeof(buf), "01234567890123456789012345678901") == CODEC_DECODE_ERROR_NOT_ACK_MSG);
    fillBuf(buf, sizeof(buf), "\"n\":\"01234567890123456789012345678901\",\"i\":2147483647");
    TEST_ASSERT(codecDecodeAck(buf, sizeof(buf), "01234567890123456789012345678901") == CODEC_DECODE_ERROR_NOT_ACK_MSG);
    fillBuf(buf, sizeof(buf), "(\"n\":\"01234567890123456789012345678901\",\"i\":2147483647)");
    TEST_ASSERT(codecDecodeAck(buf, sizeof(buf), "01234567890123456789012345678901") == CODEC_DECODE_ERROR_NOT_ACK_MSG);
    fillBuf(buf, sizeof(buf), "[\"n\":\"01234567890123456789012345678901\",\"i\":2147483647]");
    TEST_ASSERT(codecDecodeAck(buf, sizeof(buf), "01234567890123456789012345678901") == CODEC_DECODE_ERROR_NOT_ACK_MSG);
    fillBuf(buf, sizeof(buf), "{\"n\":\"01234567890123456789012345678901\"i\":2147483647}");
    TEST_ASSERT(codecDecodeAck(buf, sizeof(buf), "01234567890123456789012345678901") == CODEC_DECODE_ERROR_NOT_ACK_MSG);
    fillBuf(buf, sizeof(buf), "{\"n\":\"01234567890123456789012345678901,\"d\":2147483647}");
    TEST_ASSERT(codecDecodeAck(buf, sizeof(buf), "01234567890123456789012345678901") == CODEC_DECODE_ERROR_NOT_ACK_MSG);
    fillBuf(buf, sizeof(buf), "{\"i\":\"01234567890123456789012345678901,\"n\":2147483647}");
    TEST_ASSERT(codecDecodeAck(buf, sizeof(buf), "01234567890123456789012345678901") == CODEC_DECODE_ERROR_NOT_ACK_MSG);
    // Throw garbage ASCII at it, on the assumption that 1000 monkeys won't write a valid ack message
    for (int x = 0; x < 1000; x++) {
        for (unsigned int y = 0; y < sizeof(buf); y++) {
            buf[y] = rand()%('}' - '!') + '!';
            TEST_ASSERT(codecDecodeAck(buf, sizeof(buf), "") == CODEC_DECODE_ERROR_NOT_ACK_MSG);
        }
    }

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
    GREENTEA_SETUP(180, "default_auto");
    return verbose_test_setup_handler(number_of_cases);
}

// Test cases
Case cases[] = {
    Case("Print all data items", test_print_all_data_items),
    Case("Ack data", test_ack_data),
    Case("Random buffer lengths", test_rand),
    Case("Decode", test_decode)
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
