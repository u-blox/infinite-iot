#include "greentea-client/test_env.h"
#include "unity.h"
#include "utest.h"
#include "mbed_trace.h"
#include "mbed.h"
#include "eh_config.h"
#include "eh_i2c.h"
#include "act_si7210.h"
#include "act_magnetic.h"

using namespace utest::v1;

// These are tests for the act_si7210 Hall effect
// sensor driver.
// To run them, before you do "mbed test", you need
// to (once) do "mbedls --m 0004:UBLOX_EVK_NINA_B1" to
// set up the right target name, otherwise Mbed thinks
// you have an LPC2368 attached.
//
// ----------------------------------------------------------------
// COMPILE-TIME MACROS
// ----------------------------------------------------------------

#define TRACE_GROUP "SI7210"

// The I2C address of the SI7210
#ifdef TARGET_TB_SENSE_12
// On the Thunderboard 2 the Si7210-B-00-IV(R)/Si7210-B-01-IV(R) part
// is fitted
# define SI7210_ADDRESS SI7210_DEFAULT_ADDRESS_00_01
// On the Thunderboard 2, this peripheral is on I2C#8
# define I2C_DATA       PB8
# define I2C_CLOCK      PB9
#else
# define SI7210_ADDRESS SI7210_DEFAULT_ADDRESS
# define I2C_DATA       PIN_I2C_SDA
# define I2C_CLOCK      PIN_I2C_SCL
#endif
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

    tr_debug("Initialising SI7210...");
    x = si7210Init(SI7210_ADDRESS);
    tr_debug("Result of initialising SI7210 was %d.", x);
    TEST_ASSERT(x == ACTION_DRIVER_OK);
    si7210Deinit();

    // Shut down I2C
    i2cDeinit();

    // Capture the heap stats once more
    mbed_stats_heap_get(&statsHeapAfter);
    tr_debug("%d byte(s) of heap used at the end.", (int) statsHeapAfter.current_size);

    // The heap used should be the same as at the start
    TEST_ASSERT(statsHeapBefore.current_size == statsHeapAfter.current_size);
}

// Test of obtaining a reading
void test_reading() {
    int x = 0;
    unsigned int teslaX1000;
    mbed_stats_heap_t statsHeapBefore;
    mbed_stats_heap_t statsHeapAfter;

    tr_debug("Print something out with a float (%f) in it as tr_debug and floats allocate from the heap when first called.\n", 1.0);

    // Capture the heap stats before we start
    mbed_stats_heap_get(&statsHeapBefore);
    tr_debug("%d byte(s) of heap used at the outset.", (int) statsHeapBefore.current_size);

    // Instantiate I2C
    i2cInit(I2C_DATA, I2C_CLOCK);

    // Try to get a reading before initialisation - should fail
    TEST_ASSERT(getFieldStrength(&teslaX1000) == ACTION_DRIVER_ERROR_NOT_INITIALISED)

    tr_debug("Initialising SI7210...");
    TEST_ASSERT(si7210Init(SI7210_ADDRESS) == ACTION_DRIVER_OK);

    // Get a reading of field strength
    tr_debug("Reading SI7210...");
    x = getFieldStrength(&teslaX1000);
    tr_debug("Result of reading SI7210 is %d.", x);
    TEST_ASSERT(x == ACTION_DRIVER_OK);
    tr_debug("Field strength %.3f.", ((float) teslaX1000) / 1000);
    // Miss out the parameter
    x = getFieldStrength(NULL);
    tr_debug("Result of reading SI7210 is %d.", x);
    TEST_ASSERT(x == ACTION_DRIVER_OK);

    si7210Deinit();

    // Shut down I2C
    i2cDeinit();

    // Capture the heap stats once more
    mbed_stats_heap_get(&statsHeapAfter);
    tr_debug("%d byte(s) of heap used at the end.", (int) statsHeapAfter.current_size);

    // The heap used should be the same as at the start
    TEST_ASSERT(statsHeapBefore.current_size == statsHeapAfter.current_size);
}

// Test of changing the measurement range
void test_range() {
    int x = 0;
    unsigned int teslaX10001;
    unsigned int teslaX10002;
    mbed_stats_heap_t statsHeapBefore;
    mbed_stats_heap_t statsHeapAfter;

    tr_debug("Print something out with a float (%f) in it as tr_debug and floats allocate from the heap when first called.\n", 1.0);

    // Capture the heap stats before we start
    mbed_stats_heap_get(&statsHeapBefore);
    tr_debug("%d byte(s) of heap used at the outset.", (int) statsHeapBefore.current_size);

    // Instantiate I2C
    i2cInit(I2C_DATA, I2C_CLOCK);

    // Try to change range before initialisation - should fail
    TEST_ASSERT(setRange(RANGE_20_MICRO_TESLAS) == ACTION_DRIVER_ERROR_NOT_INITIALISED)
    TEST_ASSERT(setRange(RANGE_200_MICRO_TESLAS) == ACTION_DRIVER_ERROR_NOT_INITIALISED)

    tr_debug("Initialising SI7210...");
    TEST_ASSERT(si7210Init(SI7210_ADDRESS) == ACTION_DRIVER_OK);

    // Get a reading of field strength
    tr_debug("Reading SI7210 in 20 uTesla range...");
    x = getFieldStrength(&teslaX10001);
    tr_debug("Result of reading SI7210 is %d.", x);
    TEST_ASSERT(x == ACTION_DRIVER_OK);
    tr_debug("Field strength in 20 uTesla range %.3f.", ((float) teslaX10001) / 1000);

    // Change the range to 200 uTeslas
    tr_debug("Changing to 200 uTesla range...");
    x = setRange(RANGE_200_MICRO_TESLAS);
    tr_debug("Result of changing range is %d.", x);
    TEST_ASSERT(x == ACTION_DRIVER_OK);
    TEST_ASSERT(getRange() == RANGE_200_MICRO_TESLAS);

    // Get another reading of field strength
    tr_debug("Reading SI7210 in 200 uTesla range...");
    x = getFieldStrength(&teslaX10002);
    tr_debug("Result of reading SI7210 is %d.", x);
    TEST_ASSERT(x == ACTION_DRIVER_OK);
    tr_debug("Field strength in 200 uTesla range %.3f.", ((float) teslaX10002) / 1000);
    TEST_ASSERT(x == ACTION_DRIVER_OK);
    // The answer should be roughly similar to the first
    TEST_ASSERT_UINT_WITHIN(teslaX10001, teslaX10001, teslaX10002);

    // Change the range back to 20 uTeslas
    tr_debug("Changing back to 20 uTesla range...");
    x = setRange(RANGE_20_MICRO_TESLAS);
    tr_debug("Result of changing range is %d.", x);
    TEST_ASSERT(x == ACTION_DRIVER_OK);
    TEST_ASSERT(getRange() == RANGE_20_MICRO_TESLAS);

    // Get another reading of field strength
    tr_debug("Reading SI7210 in 20 uTesla range...");
    x = getFieldStrength(&teslaX10002);
    tr_debug("Result of reading SI7210 is %d.", x);
    TEST_ASSERT(x == ACTION_DRIVER_OK);
    tr_debug("Field strength in 20 uTesla range %.3f.", ((float) teslaX10002) / 1000);
    TEST_ASSERT(x == ACTION_DRIVER_OK);
    // The answer should be similar to the first
    TEST_ASSERT_UINT_WITHIN((teslaX10001 / 2), teslaX10001, teslaX10002);

    si7210Deinit();

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
    Case("Get field strength reading", test_reading),
    Case("Change range", test_range)
};

Specification specification(test_setup, cases);

// ----------------------------------------------------------------
// MAIN
// ----------------------------------------------------------------

int main()
{

#ifdef TARGET_TB_SENSE_12
    // If testing on the Thunderboard 2, need to power the sensor up
    // by setting PB10 high
    DigitalOut enableSi7210(PB10, 1);
    wait_ms(100);
#endif

#ifdef MBED_CONF_MBED_TRACE_ENABLE
    mbed_trace_init();

    mbed_trace_mutex_wait_function_set(lock);
    mbed_trace_mutex_release_function_set(unlock);
#endif

    // Run tests
    return !Harness::run(specification);
}

// End Of File
