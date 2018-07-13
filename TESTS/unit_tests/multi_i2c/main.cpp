#include "greentea-client/test_env.h"
#include "unity.h"
#include "utest.h"
#include "mbed_trace.h"
#include "mbed.h"
#include "act_voltages.h"
#include "eh_post.h"
#include "eh_data.h"
#include "eh_processor.h"
#include "eh_utilities.h" // for ARRAY_SIZE()
#include "act_si7210.h" // For si7210Deinit()
#include "act_lis3dh.h" // For lis3dhDeinit()

using namespace utest::v1;

// These are tests check that multiple I2C
// devices can be handled at the same time.
// They require at least a ZOEM8 GNSS device,
// an LIS3DH orientation sensor and a BME280
// temperature, humidity and pressure measuring
// device to be present on the single I2C interface
// available on the target board (with I2C pins as
// define in eh_config.h) with the I2C addresses
// as defined by eh_config.h.
// The eh_post and eh_processor modules are then
// called to perform their operations using these
// devices.
//
// ----------------------------------------------------------------
// COMPILE-TIME MACROS
// ----------------------------------------------------------------

#define TRACE_GROUP "MULTI_I2C"

// ----------------------------------------------------------------
// PRIVATE VARIABLES
// ----------------------------------------------------------------

// Lock for debug prints
static Mutex gMtx;

// An event queue for the processor
static EventQueue gWakeUpEventQueue(/* event count */ 10 * EVENTS_EVENT_SIZE);

#ifdef TARGET_UBLOX_C030_U201
// The expected desirability table after post() for
// TARGET_UBLOX_C030_U201: modem is present, and
// BME280, LIS3DH and ZOEM8 are expected to have
// been connected to I2C.
static Desirability gExpectedDesirability[] = {0, /* ACTION_TYPE_NULL */
                                               DESIRABILITY_DEFAULT, /* ACTION_TYPE_REPORT */
                                               DESIRABILITY_DEFAULT, /* ACTION_TYPE_GET_TIME_AND_REPORT */
                                               DESIRABILITY_DEFAULT, /* ACTION_TYPE_MEASURE_HUMIDITY */
                                               DESIRABILITY_DEFAULT, /* ACTION_TYPE_MEASURE_ATMOSPHERIC_PRESSURE */
                                               DESIRABILITY_DEFAULT, /* ACTION_TYPE_MEASURE_TEMPERATURE */
                                               0, /* ACTION_TYPE_MEASURE_LIGHT */
                                               DESIRABILITY_DEFAULT, /* ACTION_TYPE_MEASURE_ORIENTATION */
                                               DESIRABILITY_DEFAULT, /* ACTION_TYPE_MEASURE_POSITION */
                                               0, /* ACTION_TYPE_MEASURE_MAGNETIC */
                                               DESIRABILITY_DEFAULT /* ACTION_TYPE_MEASURE_BLE, default because it is compiled out */};
#else
//  For the main platform everything must be present
static Desirability gExpectedDesirability[] = {0, /* ACTION_TYPE_NULL */
                                               DESIRABILITY_DEFAULT, /* ACTION_TYPE_REPORT */
                                               DESIRABILITY_DEFAULT, /* ACTION_TYPE_GET_TIME_AND_REPORT */
                                               DESIRABILITY_DEFAULT, /* ACTION_TYPE_MEASURE_HUMIDITY */
                                               DESIRABILITY_DEFAULT, /* ACTION_TYPE_MEASURE_ATMOSPHERIC_PRESSURE */
                                               DESIRABILITY_DEFAULT, /* ACTION_TYPE_MEASURE_TEMPERATURE */
                                               DESIRABILITY_DEFAULT, /* ACTION_TYPE_MEASURE_LIGHT */
                                               DESIRABILITY_DEFAULT, /* ACTION_TYPE_MEASURE_ORIENTATION */
                                               DESIRABILITY_DEFAULT, /* ACTION_TYPE_MEASURE_POSITION */
                                               DESIRABILITY_DEFAULT, /* ACTION_TYPE_MEASURE_MAGNETIC */
                                               DESIRABILITY_DEFAULT /* ACTION_TYPE_MEASURE_BLE */};
#endif

// Translation table from data-type to action-type
static ActionType gDataToAction[] = {ACTION_TYPE_NULL, /* DATA_TYPE_NULL */
                                     ACTION_TYPE_REPORT, /* DATA_TYPE_CELLULAR */
                                     ACTION_TYPE_MEASURE_HUMIDITY, /* DATA_TYPE_HUMIDITY */
                                     ACTION_TYPE_MEASURE_ATMOSPHERIC_PRESSURE, /* DATA_TYPE_ATMOSPHERIC_PRESSURE */
                                     ACTION_TYPE_MEASURE_TEMPERATURE, /* DATA_TYPE_TEMPERATURE */
                                     ACTION_TYPE_MEASURE_LIGHT, /* DATA_TYPE_LIGHT */
                                     ACTION_TYPE_MEASURE_ACCELERATION, /* DATA_TYPE_ACCELERATION */
                                     ACTION_TYPE_MEASURE_POSITION, /* DATA_TYPE_POSITION */
                                     ACTION_TYPE_MEASURE_MAGNETIC, /* DATA_TYPE_MAGNETIC */
                                     ACTION_TYPE_MEASURE_BLE, /* DATA_TYPE_BLE */
                                     ACTION_TYPE_NULL, /* DATA_TYPE_WAKE_UP_REASON */
                                     ACTION_TYPE_NULL, /* DATA_TYPE_ENERGY_SOURCE */
                                     ACTION_TYPE_NULL, /* DATA_TYPE_STATISTICS */
                                     ACTION_TYPE_NULL /* DATA_TYPE_LOG */};

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

// Do some very broad range checking on data items where possible
static void rangeCheckData(Data *pData)
{
    switch (pData->type) {
        case DATA_TYPE_CELLULAR:
            tr_debug("CELLULAR: rsrp: %d dBm, rssi: %d dBm,, rsrq: %d dB, SNR: %d dB, " \
                     "cell ID: %u, transmit power: %d dBm, EARFCN:%u.",
                     pData->contents.cellular.rsrpDbm, pData->contents.cellular.rssiDbm,
                     pData->contents.cellular.rsrqDb, pData->contents.cellular.snrDb,
                     pData->contents.cellular.cellId, pData->contents.cellular.transmitPowerDbm,
                     pData->contents.cellular.earfcn);
        break;
        case DATA_TYPE_HUMIDITY:
            tr_debug("HUMIDITY: %d%%.", pData->contents.humidity.percentage);
            TEST_ASSERT(pData->contents.humidity.percentage <= 100);
        break;
        case DATA_TYPE_ATMOSPHERIC_PRESSURE:
            tr_debug("ATMOSPHERIC PRESSURE: %.2f pascal(s).",
                     (float) pData->contents.atmosphericPressure.pascalX100 / 100);
            TEST_ASSERT(pData->contents.atmosphericPressure.pascalX100 > 50000);
            TEST_ASSERT(pData->contents.atmosphericPressure.pascalX100 < 150000);
        break;
        case DATA_TYPE_TEMPERATURE:
            tr_debug("TEMPERATURE: %.2f C.", (float) pData->contents.temperature.cX100 / 100);
            TEST_ASSERT(pData->contents.temperature.cX100 > -5000);
            TEST_ASSERT(pData->contents.temperature.cX100 < 8500);
        break;
        case DATA_TYPE_LIGHT:
            tr_debug("LIGHT: %u lux, UV Index %.3f.", pData->contents.light.lux,
                     (float) pData->contents.light.uvIndexX1000 / 1000);
            TEST_ASSERT(pData->contents.light.lux < 250000);
            TEST_ASSERT(pData->contents.light.uvIndexX1000 < 15000);
        break;
        case DATA_TYPE_ACCELERATION:
            tr_debug("ACCELERATION: x: %d, y: %d, z: %d.", pData->contents.acceleration.xGX1000,
                     pData->contents.acceleration.yGX1000, pData->contents.acceleration.zGX1000);
        break;
        case DATA_TYPE_POSITION:
            tr_debug("POSITION: latitude: %.7f, longitude: %.7f, radius: %d m, altitude: %d m, speed: %u mps.",
                     (float) pData->contents.position.latitudeX10e7 / 10000000,
                     (float) pData->contents.position.longitudeX10e7 / 10000000,
                     pData->contents.position.radiusMetres,
                     pData->contents.position.altitudeMetres,
                     pData->contents.position.speedMPS);
            TEST_ASSERT(pData->contents.position.radiusMetres < 50000);
            TEST_ASSERT(pData->contents.position.altitudeMetres < 2000);
            TEST_ASSERT(pData->contents.position.speedMPS < 10);
        break;
        case DATA_TYPE_MAGNETIC:
            tr_debug("MAGNETIC: %.3f Tesla", (float) pData->contents.magnetic.teslaX1000 / 1000);
            TEST_ASSERT(pData->contents.magnetic.teslaX1000 < 4000);
        break;
        case DATA_TYPE_BLE:
            tr_debug("BLE: device: \"%s\", battery: %u%%.", pData->contents.ble.name,
                     pData->contents.ble.batteryPercentage);
            TEST_ASSERT(pData->contents.ble.batteryPercentage <= 100);
        break;
        default:
            tr_debug("UNHANDLED DATA TYPE (%d).", pData->type);
        break;
    }
}

// ----------------------------------------------------------------
// TESTS
// ----------------------------------------------------------------

// Test of power-on self-test
void test_post() {
    mbed_stats_heap_t statsHeapBefore;
    mbed_stats_heap_t statsHeapAfter;
    Desirability d;
    Timer timer;

    tr_debug("Print something with a float in it (%f) as that seems to allocate from the heap when first called.\n", 1.0);

    // Initialise things
    actionInit();
    processorInit();

    // Capture the heap stats before we start
    mbed_stats_heap_get(&statsHeapBefore);
    tr_debug("%d byte(s) of heap used at the outset.", (int) statsHeapBefore.current_size);

    // Check that we can initialise things, tolerating failures
    // since not everything might be attached in all cases
    timer.reset();
    timer.start();
    tr_debug("Calling POST...");
    TEST_ASSERT(post(true) == POST_RESULT_OK);
    timer.stop();

    tr_debug("That took %.3f seconds.", (float) timer.read_ms() / 1000);

    // Now check which things have been marked as undesirable
    // due to not being present
    for (int x = ACTION_TYPE_NULL + 1; x < MAX_NUM_ACTION_TYPES; x++) {
        d = actionGetDesirability((ActionType) x);
        tr_debug("Action type %d, expected to have desirability %d, has desirability %d.",
                 x, (int) gExpectedDesirability[x], (int) d);
        TEST_ASSERT(gExpectedDesirability[x] == d);
    }

    // Capture the heap stats once more
    mbed_stats_heap_get(&statsHeapAfter);
    tr_debug("%d byte(s) of heap used at the end.", (int) statsHeapAfter.current_size);

    // The heap used should be the same as at the start
    TEST_ASSERT(statsHeapBefore.current_size == statsHeapAfter.current_size);
}

// Test of taking readings using eh_processor from multiple devices at once
void test_readings() {
    mbed_stats_heap_t statsHeapBefore;
    mbed_stats_heap_t statsHeapAfter;
    Data *pData;
    int numExpected = 0;
    Desirability d[MAX_NUM_ACTION_TYPES];
    Timer timer;

    tr_debug("Print something with a float in it (%f) as that seems to allocate from the heap when first called.\n", 1.0);

    // Do not initialise things, that was already done in test_post().

    // Capture the heap stats before we start
    mbed_stats_heap_get(&statsHeapBefore);
    tr_debug("%d byte(s) of heap used at the outset.", (int) statsHeapBefore.current_size);

    // Again, no need for POST, that was already done above,
    // but need to pretend that power is good
    voltageFakeIsGood(true);
    // Switch off reporting (so that the data queue is not emptied) and
    // BLE ('cos it goes on forever).
    TEST_ASSERT(actionSetDesirability(ACTION_TYPE_REPORT, 0));
    gExpectedDesirability[ACTION_TYPE_REPORT] = 0;
    TEST_ASSERT(actionSetDesirability(ACTION_TYPE_GET_TIME_AND_REPORT, 0));
    gExpectedDesirability[ACTION_TYPE_GET_TIME_AND_REPORT] = 0;
    TEST_ASSERT(actionSetDesirability(ACTION_TYPE_MEASURE_BLE, 0));
    gExpectedDesirability[ACTION_TYPE_MEASURE_BLE] = 0;

    // Work out the number of expected actions
    for (int x = ACTION_TYPE_NULL + 1; x < MAX_NUM_ACTION_TYPES; x++) {
        if (gExpectedDesirability[x] > 0) {
            numExpected++;
        }
    }
    // Now call processor, should result in actions being
    // performed and data assembled
    tr_debug("Calling processor...");
    timer.reset();
    timer.start();
    processorHandleWakeup(&gWakeUpEventQueue);
    timer.stop();
    tr_debug("That took %.3f seconds.", (float) timer.read_ms() / 1000);

    // When done, there should be a data item in the queue for each
    // of the expected action types and none for the non-expected
    // action types
    memset(d, 0, sizeof(d));
    pData = pDataSort();
    for (int x = ACTION_TYPE_NULL + 1; (x < MAX_NUM_ACTION_TYPES) && (pData != NULL); x++) {
        TEST_ASSERT(pData->type < ARRAY_SIZE(gDataToAction));
        d[gDataToAction[pData->type]] = DESIRABILITY_DEFAULT;
        rangeCheckData(pData);
        // Free the data items as we go
        dataFree(&pData);
        pData = pDataNext();
    }
    for (int x = ACTION_TYPE_NULL + 1; x < MAX_NUM_ACTION_TYPES; x++) {
        tr_debug("Action type %d, expected %d, has %d.",
                 x, (int) gExpectedDesirability[x], (int) d[x]);
        // Since we can't get a GNSS fix in most test environments,
        // don't check for a reading from it here
        if (x != ACTION_TYPE_MEASURE_POSITION) {
            TEST_ASSERT(d[x] == gExpectedDesirability[x]);
        }
    }

    // Capture the heap stats once more
    mbed_stats_heap_get(&statsHeapAfter);
    tr_debug("%d byte(s) of heap used at the end.", (int) statsHeapAfter.current_size);

    // The heap used should be the same as at the start
    TEST_ASSERT(statsHeapBefore.current_size == statsHeapAfter.current_size);

    // Stop the fakery
    voltageFakeIsGood(false);
}

// As test_readings() but with multiple attempts including GNSS
void test_readings_loop_gnss() {
    mbed_stats_heap_t statsHeapBefore;
    mbed_stats_heap_t statsHeapAfter;
    Data *pData;
    int numLoops = 3;
    int numExpected = 0;
    int n[MAX_NUM_ACTION_TYPES];
    Timer timer;

    tr_debug("Print something with a float in it (%f) as that seems to allocate from the heap when first called.\n", 1.0);

    // Do not initialise things, that was already done in test_post().

    // Capture the heap stats before we start
    mbed_stats_heap_get(&statsHeapBefore);
    tr_debug("%d byte(s) of heap used at the outset.", (int) statsHeapBefore.current_size);

    // Again, no need for POST, that was already done above,
    // but need to pretend that power is good
    voltageFakeIsGood(true);

    // Work out the number of expected actions
    for (int x = ACTION_TYPE_NULL + 1; x < MAX_NUM_ACTION_TYPES; x++) {
        if (gExpectedDesirability[x] > 0) {
            numExpected++;
        }
    }
    // Now call processor multiple times, which should result in actions being
    // performed and data assembled
    tr_debug("Calling processor %d time(s)...", numLoops);
    for (int x = 0; x < numLoops; x++) {
        timer.reset();
        timer.start();
        processorHandleWakeup(&gWakeUpEventQueue);
        timer.stop();
        tr_debug("Iteration %d took %.3f second(s).", x + 1, (float) timer.read_ms() / 1000);
    }

    // When done, there should be numLoops data items in the queue for each
    // of the expected action types and none for the non-expected
    // action types
    memset(n, 0, sizeof(n));
    pData = pDataSort();
    while (pData != NULL) {
        TEST_ASSERT(pData->type < ARRAY_SIZE(n));
        n[gDataToAction[pData->type]]++;
        rangeCheckData(pData);
        // Free the data items as we go
        dataFree(&pData);
        pData = pDataNext();
    }
    for (int x = ACTION_TYPE_NULL + 1; x < MAX_NUM_ACTION_TYPES; x++) {
        if (gExpectedDesirability[x] > 0) {
            tr_debug("Action type %d, expected %d reading(s), has %d.",
                    x, numLoops, n[x]);
            if (x != ACTION_TYPE_MEASURE_POSITION) {
                TEST_ASSERT(n[x] == numLoops);
            }
        } else {
            tr_debug("Action type %d, expected no readings, has %d.", x, n[x]);
            if (x != ACTION_TYPE_MEASURE_POSITION) {
                TEST_ASSERT(n[x] == 0);
            }
        }
    }

    // Capture the heap stats once more
    mbed_stats_heap_get(&statsHeapAfter);
    tr_debug("%d byte(s) of heap used at the end.", (int) statsHeapAfter.current_size);

    // The heap used should be the same as at the start
    TEST_ASSERT(statsHeapBefore.current_size == statsHeapAfter.current_size);

    // Stop the fakery
    voltageFakeIsGood(false);
}

// As test_readings_loop_gnss() but taking out GNSS so that we can take a lot of
// the other readings in a sensible time-frame
void test_readings_loop_no_gnss() {
    mbed_stats_heap_t statsHeapBefore;
    mbed_stats_heap_t statsHeapAfter;
    Data *pData;
    int numLoops = 60;
    int numExpected = 0;
    int n[MAX_NUM_ACTION_TYPES];
    Timer timer;

    tr_debug("Print something with a float in it (%f) as that seems to allocate from the heap when first called.\n", 1.0);

    // Do not initialise things, that was already done in test_post().

    // Capture the heap stats before we start
    mbed_stats_heap_get(&statsHeapBefore);
    tr_debug("%d byte(s) of heap used at the outset.", (int) statsHeapBefore.current_size);

    // Again, no need for POST, that was already done above,
    // but need to pretend that power is good
    voltageFakeIsGood(true);

    // Switch off GNSS
    TEST_ASSERT(actionSetDesirability(ACTION_TYPE_MEASURE_POSITION, 0));
    gExpectedDesirability[ACTION_TYPE_MEASURE_POSITION] = 0;

    // Work out the number of expected actions
    for (int x = ACTION_TYPE_NULL + 1; x < MAX_NUM_ACTION_TYPES; x++) {
        if (gExpectedDesirability[x] > 0) {
            numExpected++;
        }
    }
    // Now call processor multiple times, which should result in actions being
    // performed and data assembled
    tr_debug("Calling processor %d time(s)...", numLoops);
    for (int x = 0; x < numLoops; x++) {
        timer.reset();
        timer.start();
        processorHandleWakeup(&gWakeUpEventQueue);
        timer.stop();
        tr_debug("Iteration %d took %.3f second(s).", x + 1, (float) timer.read_ms() / 1000);
    }

    // When done, there should be numLoops data items in the queue for each
    // of the expected action types and none for the non-expected
    // action types
    memset(n, 0, sizeof(n));
    pData = pDataSort();
    while (pData != NULL) {
        TEST_ASSERT(pData->type < ARRAY_SIZE(n));
        n[gDataToAction[pData->type]]++;
        rangeCheckData(pData);
        // Free the data items as we go
        dataFree(&pData);
        pData = pDataNext();
    }
    for (int x = ACTION_TYPE_NULL + 1; x < MAX_NUM_ACTION_TYPES; x++) {
        if (gExpectedDesirability[x] > 0) {
            tr_debug("Action type %d, expected %d reading(s), has %d.",
                     x, numLoops, n[x]);
            TEST_ASSERT(n[x] == numLoops);
        } else {
            tr_debug("Action type %d, expected no readings, has %d.", x, n[x]);
            TEST_ASSERT(n[x] == 0);
        }
    }

    // These two are normally left on so de-initialise them here
    // now that we've finished all of the testing
    si7210Deinit();
    lis3dhDeinit();

    // Capture the heap stats once more
    mbed_stats_heap_get(&statsHeapAfter);
    tr_debug("%d byte(s) of heap used at the end.", (int) statsHeapAfter.current_size);

    // The heap used should be the same as at the start
    TEST_ASSERT(statsHeapBefore.current_size == statsHeapAfter.current_size);

    // Stop the fakery
    voltageFakeIsGood(false);
}

// ----------------------------------------------------------------
// TEST ENVIRONMENT
// ----------------------------------------------------------------

// Setup the test environment
utest::v1::status_t test_setup(const size_t number_of_cases) {
    // Setup Greentea with a nice long timeout
    // since GNSS will be trying to get a fix (and probably
    // be unable to), timing out at 60 seconds for each try
    GREENTEA_SETUP(360, "default_auto");
    return verbose_test_setup_handler(number_of_cases);
}

// Test cases: these MUST be run in the order given
Case cases[] = {
    Case("POST", test_post),
    Case("Readings", test_readings),
    Case("Readings loop with GNSS", test_readings_loop_gnss),
    Case("Readings loop without GNSS", test_readings_loop_no_gnss)
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
