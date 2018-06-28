#include "greentea-client/test_env.h"
#include "unity.h"
#include "utest.h"
#include "mbed_trace.h"
#include "mbed.h"
#include "eh_config.h"
#include "eh_i2c.h"
#include "act_zoem8.h"
#include "act_position.h"

using namespace utest::v1;

// These are tests for the act_zoem8 GNSS driver.
// To run them, before you do "mbed test", you need
// to (once) do "mbedls --m 0004:UBLOX_EVK_NINA_B1" to
// set up the right target name, otherwise Mbed thinks
// you have an LPC2368 attached.
//
// ----------------------------------------------------------------
// COMPILE-TIME MACROS
// ----------------------------------------------------------------

#define TRACE_GROUP "ZOEM8"

#define ZOEM8_ADDRESS  ZOEM8_DEFAULT_ADDRESS
#define I2C_DATA       PIN_I2C_SDA
#define I2C_CLOCK      PIN_I2C_SCL

// ----------------------------------------------------------------
// PRIVATE VARIABLES
// ----------------------------------------------------------------

// Lock for debug prints
static Mutex gMtx;

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

// ----------------------------------------------------------------
// TESTS
// ----------------------------------------------------------------

// Test of initialisation
void test_init() {
    int x = 0;
    mbed_stats_heap_t statsHeapBefore;
    mbed_stats_heap_t statsHeapAfter;

    tr_debug("Print something out as tr_debug seems to allocate from the heap when first called.\n");

    // Capture the heap stats before we start
    mbed_stats_heap_get(&statsHeapBefore);
    tr_debug("%d byte(s) of heap used at the outset.", (int) statsHeapBefore.current_size);

    // Instantiate I2C
    i2cInit(I2C_DATA, I2C_CLOCK);

    tr_debug("Initialising ZOEM8...");
    x = zoem8Init(ZOEM8_ADDRESS);
    tr_debug("Result of initialising ZOEM8 was %d.", x);
    TEST_ASSERT(x == ACTION_DRIVER_OK);
    zoem8Deinit();

    // Shut down I2C
    i2cDeinit();

    // Capture the heap stats once more
    mbed_stats_heap_get(&statsHeapAfter);
    tr_debug("%d byte(s) of heap used at the end.", (int) statsHeapAfter.current_size);

    // The heap used should be the same as at the start
    TEST_ASSERT(statsHeapBefore.current_size == statsHeapAfter.current_size);
}

// Test of obtaining position readings
void test_position_readings() {
    int x = 0;
    int latitudeX10e7 = 0;
    int longitudeX10e7 = 0;
    int radiusMetres = 0;
    int altitudeMetres = 0;
    unsigned char speedMPS = 0;
    unsigned char svs = 0;
    mbed_stats_heap_t statsHeapBefore;
    mbed_stats_heap_t statsHeapAfter;

    tr_debug("Print something out with a float (%f) in it as tr_debug and floats allocate from the heap when first called.\n", 1.0);

    // Capture the heap stats before we start
    mbed_stats_heap_get(&statsHeapBefore);
    tr_debug("%d byte(s) of heap used at the outset.", (int) statsHeapBefore.current_size);

    // Instantiate I2C
    i2cInit(I2C_DATA, I2C_CLOCK);

    // Try to get a reading before initialisation - should fail
    TEST_ASSERT(getPosition(&latitudeX10e7, &longitudeX10e7,
                           &radiusMetres, &altitudeMetres,
                           &speedMPS, &svs) == ACTION_DRIVER_ERROR_NOT_INITIALISED)

    tr_debug("Initialising ZOEM8...");
    TEST_ASSERT(zoem8Init(ZOEM8_ADDRESS) == ACTION_DRIVER_OK);

    // Make sure there's time for ZOE to start up and provide readings of some form
    wait_ms(1000);

    // Get a position reading 10 times (to check I2C interface timing)
    for (int y = 0; y < 10; y++) {
        tr_debug("Reading position...");
        x = getPosition(&latitudeX10e7, &longitudeX10e7,
                        &radiusMetres, &altitudeMetres,
                        &speedMPS, &svs);
        tr_debug("Result of reading position is %d.", x);
        // Depending on whether we can see satellites the answer may be no valid data or may be OK
        TEST_ASSERT((x == ACTION_DRIVER_OK) || (x == ACTION_DRIVER_ERROR_NO_VALID_DATA));
        if (x == ACTION_DRIVER_OK) {
            tr_debug("Latitude %3.6f, longitude %3.6f, radius %d metre(s), altitude %d metre(s), speed %d metres/second, %d SV(s).",
                      ((float) latitudeX10e7) / 10000000, ((float) longitudeX10e7) / 10000000, radiusMetres,
                      altitudeMetres, speedMPS, svs);
            TEST_ASSERT(radiusMetres < 50000);
            TEST_ASSERT(altitudeMetres < 2000);
            TEST_ASSERT(speedMPS < 10);
            TEST_ASSERT(svs < 64);
        }
    }

    // Repeat with null parameters in a few combinations
    x = getPosition(NULL, NULL, NULL, NULL, NULL, NULL);
    TEST_ASSERT((x == ACTION_DRIVER_OK) || (x == ACTION_DRIVER_ERROR_NO_VALID_DATA));
    x = getPosition(&latitudeX10e7, &longitudeX10e7, NULL, NULL, NULL, NULL);
    TEST_ASSERT((x == ACTION_DRIVER_OK) || (x == ACTION_DRIVER_ERROR_NO_VALID_DATA));
    x = getPosition(&latitudeX10e7, &longitudeX10e7, &radiusMetres, NULL, NULL, NULL);
    TEST_ASSERT((x == ACTION_DRIVER_OK) || (x == ACTION_DRIVER_ERROR_NO_VALID_DATA));
    x = getPosition(&latitudeX10e7, &longitudeX10e7, &radiusMetres, &altitudeMetres, NULL, NULL);
    TEST_ASSERT((x == ACTION_DRIVER_OK) || (x == ACTION_DRIVER_ERROR_NO_VALID_DATA));

    zoem8Deinit();

    // Shut down I2C
    i2cDeinit();

    // Capture the heap stats once more
    mbed_stats_heap_get(&statsHeapAfter);
    tr_debug("%d byte(s) of heap used at the end.", (int) statsHeapAfter.current_size);

    // The heap used should be the same as at the start
    TEST_ASSERT(statsHeapBefore.current_size == statsHeapAfter.current_size);
}

// Test of obtaining time readings
void test_time_readings() {
    int x = 0;
    mbed_stats_heap_t statsHeapBefore;
    mbed_stats_heap_t statsHeapAfter;
    struct tm *localTime;
    char timeString[25];
    time_t timeUtc;

    tr_debug("Print something out as tr_debug allocate from the heap when first called.\n");

    // Capture the heap stats before we start
    mbed_stats_heap_get(&statsHeapBefore);
    tr_debug("%d byte(s) of heap used at the outset.", (int) statsHeapBefore.current_size);

    // Instantiate I2C
    i2cInit(I2C_DATA, I2C_CLOCK);

    // Try to get a reading before initialisation - should fail
    TEST_ASSERT(getTime(&timeUtc) == ACTION_DRIVER_ERROR_NOT_INITIALISED)

    tr_debug("Initialising ZOEM8...");
    TEST_ASSERT(zoem8Init(ZOEM8_ADDRESS) == ACTION_DRIVER_OK);

    // Make sure there's time for ZOE to start up and provide readings of some form
    wait_ms(1000);

    // Get a position reading 10 times (to check I2C interface timing)
    for (int y = 0; y < 10; y++) {
        tr_debug("Reading time...");
        x = getTime(&timeUtc);
        tr_debug("Result of reading time is %d.", x);
        // Depending on whether we can see satellites the answer may be no valid data or may be OK
        TEST_ASSERT((x == ACTION_DRIVER_OK) || (x == ACTION_DRIVER_ERROR_NO_VALID_DATA));
        if (x == ACTION_DRIVER_OK) {
            localTime = localtime(&timeUtc);
            if (localTime) {
                if (strftime(timeString, sizeof(timeString), "%a %b %d %H:%M:%S %Y", localTime) > 0) {
                    tr_debug("GNSS timestamp is %s.\n", timeString);
                }
            }
        }
    }

    // Repeat with null parameter
    x = getTime(NULL);
    TEST_ASSERT((x == ACTION_DRIVER_OK) || (x == ACTION_DRIVER_ERROR_NO_VALID_DATA));

    zoem8Deinit();

    // Shut down I2C
    i2cDeinit();

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
    GREENTEA_SETUP(60, "default_auto");
    return verbose_test_setup_handler(number_of_cases);
}

// Test cases
Case cases[] = {
    Case("Initialisation", test_init),
    Case("Take position readings", test_position_readings),
    Case("Take time readings", test_time_readings)
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
