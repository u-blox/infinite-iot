#include "greentea-client/test_env.h"
#include "unity.h"
#include "utest.h"
#include "mbed_trace.h"
#include "mbed.h"
#include "eh_config.h"
#include "eh_i2c.h"
#include "act_lis3dh.h"
#include "act_orientation.h"
#define TRACE_GROUP "PROCESSOR"

using namespace utest::v1;

// These are tests for the act_lis3dh orientation
// sensor driver.
// To run them, before you do "mbed test", you need
// to (once) do "mbedls --m 0004:UBLOX_EVK_NINA_B1" to
// set up the right target name, otherwise Mbed thinks
// you have an LPC2368 attached.
//
// ----------------------------------------------------------------
// COMPILE-TIME MACROS
// ----------------------------------------------------------------

#define LIS3DH_ADDRESS LIS3DH_DEFAULT_ADDRESS
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

    tr_debug("Initialising LIS3DH...");
    x = lis3dhInit(LIS3DH_ADDRESS);
    tr_debug("Result of initialising LIS3DH was %d.", x);
    TEST_ASSERT(x == ACTION_DRIVER_OK);
    lis3dhDeinit();

    // Shut down I2C
    i2cDeinit();

    // Capture the heap stats once more
    mbed_stats_heap_get(&statsHeapAfter);
    tr_debug("%d byte(s) of heap used at the end.", (int) statsHeapAfter.current_size);

    // The heap used should be the same as at the start
    TEST_ASSERT(statsHeapBefore.current_size == statsHeapAfter.current_size);
}

// Test of obtaining an orientation reading
void test_reading() {
    int x = 0;
    int readingX = 0;
    int readingY = 0;
    int readingZ = 0;
    int readingX1 = 0;
    int readingY1 = 0;
    int readingZ1 = 0;
    mbed_stats_heap_t statsHeapBefore;
    mbed_stats_heap_t statsHeapAfter;

    tr_debug("Print something out as tr_debug seems to allocate from the heap when first called.\n");

    // Capture the heap stats before we start
    mbed_stats_heap_get(&statsHeapBefore);
    tr_debug("%d byte(s) of heap used at the outset.", (int) statsHeapBefore.current_size);

    // Instantiate I2C
    i2cInit(I2C_DATA, I2C_CLOCK);

    // Try to take a reading before initialisation - should fail
    TEST_ASSERT(getOrientation(&readingX, &readingY, &readingZ) == ACTION_DRIVER_ERROR_NOT_INITIALISED)

    tr_debug("Initialising LIS3DH...");
    TEST_ASSERT(lis3dhInit(LIS3DH_ADDRESS) == ACTION_DRIVER_OK);

    // Get an orientation reading
    tr_debug("Reading LIS3DH...");
    x = getOrientation(&readingX, &readingY, &readingZ);
    tr_debug("Result of reading LIS3DH is %d.", x);
    TEST_ASSERT(x == ACTION_DRIVER_OK);
    tr_debug("Orientation is x: %d, y: %d, z: %d.", readingX, readingY, readingZ);

    // Repeat leaving parameters as NULL in various combinations
    // The answers should be roughly similar to the first
    TEST_ASSERT(getOrientation(&readingX1, &readingY1, NULL) == ACTION_DRIVER_OK)
    TEST_ASSERT_INT_WITHIN(readingX / 5, readingX, readingX1);
    TEST_ASSERT_INT_WITHIN(readingY / 5, readingY, readingY1);
    readingX1 = 0;
    TEST_ASSERT(getOrientation(&readingX1, NULL, NULL) == ACTION_DRIVER_OK)
    TEST_ASSERT_INT_WITHIN(readingX / 5, readingX, readingX1);
    TEST_ASSERT(getOrientation(NULL, NULL, NULL) == ACTION_DRIVER_OK)
    readingY1 = 0;
    readingZ1 = 0;
    TEST_ASSERT(getOrientation(NULL, &readingY1, &readingZ1) == ACTION_DRIVER_OK)
    TEST_ASSERT_INT_WITHIN(readingY / 5, readingY, readingY1);
    TEST_ASSERT_INT_WITHIN(readingZ / 5, readingZ, readingZ1);
    readingZ1 = 0;
    TEST_ASSERT(getOrientation(NULL, NULL, &readingZ1) == ACTION_DRIVER_OK)
    TEST_ASSERT_INT_WITHIN(readingZ / 5, readingZ, readingZ1);
    readingY1 = 0;
    TEST_ASSERT(getOrientation(NULL, &readingY1, NULL) == ACTION_DRIVER_OK)
    TEST_ASSERT_INT_WITHIN(readingY / 5, readingY, readingY1);
    readingX1 = 0;
    readingZ1 = 0;
    TEST_ASSERT(getOrientation(&readingX1, NULL, &readingZ1) == ACTION_DRIVER_OK)
    TEST_ASSERT_INT_WITHIN(readingX / 5, readingX, readingX1);
    TEST_ASSERT_INT_WITHIN(readingZ / 5, readingZ, readingZ1);

    lis3dhDeinit();

    // Shut down I2C
    i2cDeinit();

    // Capture the heap stats once more
    mbed_stats_heap_get(&statsHeapAfter);
    tr_debug("%d byte(s) of heap used at the end.", (int) statsHeapAfter.current_size);

    // The heap used should be the same as at the start
    TEST_ASSERT(statsHeapBefore.current_size == statsHeapAfter.current_size);
}

// TODO: test interrupt in

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
    Case("Get orientation", test_reading),
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
