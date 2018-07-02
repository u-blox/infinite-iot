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
    for (int y = 0; y < 10; y++) {
        tr_debug("Reading SI7210...");
        x = getFieldStrength(&teslaX1000);
        tr_debug("Result of reading SI7210 is %d.", x);
        TEST_ASSERT(x == ACTION_DRIVER_OK);
        tr_debug("Field strength %.3f.", ((float) teslaX1000) / 1000);
        // Range check
        TEST_ASSERT(teslaX1000 < 4000);
    }
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
    unsigned int teslaX1000;
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
    x = getFieldStrength(&teslaX1000);
    tr_debug("Result of reading SI7210 is %d.", x);
    TEST_ASSERT(x == ACTION_DRIVER_OK);
    tr_debug("Field strength in 20 uTesla range %.3f.", ((float) teslaX1000) / 1000);
    // Range check
    TEST_ASSERT(teslaX1000 < 1000);

    // Change the range to 200 uTeslas
    tr_debug("Changing to 200 uTesla range...");
    x = setRange(RANGE_200_MICRO_TESLAS);
    tr_debug("Result of changing range is %d.", x);
    TEST_ASSERT(x == ACTION_DRIVER_OK);
    TEST_ASSERT(getRange() == RANGE_200_MICRO_TESLAS);

    // Get another reading of field strength
    tr_debug("Reading SI7210 in 200 uTesla range...");
    x = getFieldStrength(&teslaX1000);
    tr_debug("Result of reading SI7210 is %d.", x);
    TEST_ASSERT(x == ACTION_DRIVER_OK);
    tr_debug("Field strength in 200 uTesla range %.3f.", ((float) teslaX1000) / 1000);
    TEST_ASSERT(x == ACTION_DRIVER_OK);
    // Range check
    TEST_ASSERT(teslaX1000 < 1000);

    // Change the range back to 20 uTeslas
    tr_debug("Changing back to 20 uTesla range...");
    x = setRange(RANGE_20_MICRO_TESLAS);
    tr_debug("Result of changing range is %d.", x);
    TEST_ASSERT(x == ACTION_DRIVER_OK);
    TEST_ASSERT(getRange() == RANGE_20_MICRO_TESLAS);

    // Get another reading of field strength
    tr_debug("Reading SI7210 in 20 uTesla range...");
    x = getFieldStrength(&teslaX1000);
    tr_debug("Result of reading SI7210 is %d.", x);
    TEST_ASSERT(x == ACTION_DRIVER_OK);
    tr_debug("Field strength in 20 uTesla range %.3f.", ((float) teslaX1000) / 1000);
    TEST_ASSERT(x == ACTION_DRIVER_OK);
    // Range check
    TEST_ASSERT(teslaX1000 < 1000);

    si7210Deinit();

    // Shut down I2C
    i2cDeinit();

    // Capture the heap stats once more
    mbed_stats_heap_get(&statsHeapAfter);
    tr_debug("%d byte(s) of heap used at the end.", (int) statsHeapAfter.current_size);

    // The heap used should be the same as at the start
    TEST_ASSERT(statsHeapBefore.current_size == statsHeapAfter.current_size);
}

// Test of setting up the interrupt (noting that this doesn't check if it goes off)
void test_interrupt() {
    int x = 0;
    unsigned int thresholdTeslaX10001 = 0;
    unsigned int hysteresisTeslaX10001 = 0;
    bool activeHigh1 = false;
    unsigned int thresholdTeslaX10002 = 0;
    unsigned int hysteresisTeslaX10002 = 0;
    bool activeHigh2 = false;
    mbed_stats_heap_t statsHeapBefore;
    mbed_stats_heap_t statsHeapAfter;

    tr_debug("Print something out with a float (%f) in it as tr_debug and floats allocate from the heap when first called.\n", 1.0);

    // Capture the heap stats before we start
    mbed_stats_heap_get(&statsHeapBefore);
    tr_debug("%d byte(s) of heap used at the outset.", (int) statsHeapBefore.current_size);

    // Instantiate I2C
    i2cInit(I2C_DATA, I2C_CLOCK);

    // Try to set up the interupt before initialisation - should fail
    TEST_ASSERT(setInterrupt(thresholdTeslaX10001, hysteresisTeslaX10001, activeHigh1) == ACTION_DRIVER_ERROR_NOT_INITIALISED)
    TEST_ASSERT(getInterrupt(&thresholdTeslaX10001, &hysteresisTeslaX10001, &activeHigh1) == ACTION_DRIVER_ERROR_NOT_INITIALISED)

    tr_debug("Initialising SI7210...");
    TEST_ASSERT(si7210Init(SI7210_ADDRESS) == ACTION_DRIVER_OK);

    // Get the current interrupt settings
    tr_debug("Get interrupt settings...");
    x = getInterrupt(&thresholdTeslaX10001, &hysteresisTeslaX10001, &activeHigh1);
    tr_debug("Result of reading SI7210 is %d.", x);
    TEST_ASSERT(x == ACTION_DRIVER_OK);
    tr_debug("Interrupt threshold is %.3f.", ((float) thresholdTeslaX10001) / 1000);
    tr_debug("Hysteresis is %.3f.", ((float) hysteresisTeslaX10001) / 1000);
    tr_debug("Active high is %s.", activeHigh1 ? "true" : "false");

    // From the si7210 data sheet:
    // Threshold can be 0 or 80 to 19200 for the 20 micro-Tesla range (x10 for 200 micro-Tesla range)
    // If threshold > 0 then Hysteresis can be 80 to 17920 for the 20 micro-Tesla range (x10 for 200 micro-Tesla range)
    // else Hysteresis can be 40 to 8960 for the 20 micro-Tesla range (x10 for 200 micro-Tesla range)

    // Try with a threshold of 0, and the hysteresis below its minimum of 80, first
    thresholdTeslaX10001 = 0;
    hysteresisTeslaX10001 = 79;
    activeHigh1 = (rand() % 2 > 0) ? true : false;
    // Set the interrupt settings
    tr_debug("Set interrupt settings with threshold %d, hysteresis %d...", thresholdTeslaX10001, hysteresisTeslaX10001);
    x = setInterrupt(thresholdTeslaX10001, hysteresisTeslaX10001, activeHigh1);
    tr_debug("Result of setting SI7210 is %d.", x);
    TEST_ASSERT(x == ACTION_DRIVER_OK);
    // Read the interrupt settings again
    tr_debug("Get interrupt settings...");
    x = getInterrupt(&thresholdTeslaX10002, &hysteresisTeslaX10002, &activeHigh2);
    tr_debug("Result of reading SI7210 is %d.", x);
    tr_debug("Interrupt threshold is %.3f.", ((float) thresholdTeslaX10002) / 1000);
    TEST_ASSERT(thresholdTeslaX10002 == thresholdTeslaX10001);
    tr_debug("Hysteresis is %.3f.", ((float) hysteresisTeslaX10002) / 1000);
    TEST_ASSERT(hysteresisTeslaX10002 == 80);
    tr_debug("Active high is %s.", activeHigh2 ? "true" : "false");
    TEST_ASSERT(activeHigh2 == activeHigh1);

    // Try again with threshold of 0 and hysteresis above its maximum of 17920
    thresholdTeslaX10001 = 0;
    hysteresisTeslaX10001 = 17921;
    activeHigh1 = (rand() % 2 > 0) ? true : false;
    // Set the interrupt settings
    tr_debug("Set interrupt settings with threshold %d, hysteresis %d...", thresholdTeslaX10001, hysteresisTeslaX10001);
    x = setInterrupt(thresholdTeslaX10001, hysteresisTeslaX10001, activeHigh1);
    tr_debug("Result of setting SI7210 is %d.", x);
    TEST_ASSERT(x == ACTION_DRIVER_OK);
    // Read the interrupt settings again
    tr_debug("Get interrupt settings...");
    x = getInterrupt(&thresholdTeslaX10002, &hysteresisTeslaX10002, &activeHigh2);
    tr_debug("Result of reading SI7210 is %d.", x);
    tr_debug("Interrupt threshold is %.3f.", ((float) thresholdTeslaX10002) / 1000);
    TEST_ASSERT(thresholdTeslaX10002 == thresholdTeslaX10001);
    tr_debug("Hysteresis is %.3f.", ((float) hysteresisTeslaX10002) / 1000);
    TEST_ASSERT(hysteresisTeslaX10002 == 17920);
    tr_debug("Active high is %s.", activeHigh2 ? "true" : "false");
    TEST_ASSERT(activeHigh2 == activeHigh1);

    // Try with a threshold above its maximum of 80, and the hysteresis above its minimum of 40
    thresholdTeslaX10001 = 19201;
    hysteresisTeslaX10001 = 39;
    activeHigh1 = (rand() % 2 > 0) ? true : false;
    // Set the interrupt settings
    tr_debug("Set interrupt settings with threshold %d, hysteresis %d...", thresholdTeslaX10001, hysteresisTeslaX10001);
    x = setInterrupt(thresholdTeslaX10001, hysteresisTeslaX10001, activeHigh1);
    tr_debug("Result of setting SI7210 is %d.", x);
    TEST_ASSERT(x == ACTION_DRIVER_OK);
    // Read the interrupt settings again
    tr_debug("Get interrupt settings...");
    x = getInterrupt(&thresholdTeslaX10002, &hysteresisTeslaX10002, &activeHigh2);
    tr_debug("Result of reading SI7210 is %d.", x);
    tr_debug("Interrupt threshold is %.3f.", ((float) thresholdTeslaX10002) / 1000);
    TEST_ASSERT(thresholdTeslaX10002 == 19200);
    tr_debug("Hysteresis is %.3f.", ((float) hysteresisTeslaX10002) / 1000);
    TEST_ASSERT(hysteresisTeslaX10002 == 40);
    tr_debug("Active high is %s.", activeHigh2 ? "true" : "false");
    TEST_ASSERT(activeHigh2 == activeHigh1);

    // Try with a threshold below its minimum of 80, and the hysteresis above its maximum of 8960
    thresholdTeslaX10001 = 79;
    hysteresisTeslaX10001 = 8961;
    activeHigh1 = (rand() % 2 > 0) ? true : false;
    // Set the interrupt settings
    tr_debug("Set interrupt settings with threshold %d, hysteresis %d...", thresholdTeslaX10001, hysteresisTeslaX10001);
    x = setInterrupt(thresholdTeslaX10001, hysteresisTeslaX10001, activeHigh1);
    tr_debug("Result of setting SI7210 is %d.", x);
    TEST_ASSERT(x == ACTION_DRIVER_OK);
    // Read the interrupt settings again
    tr_debug("Get interrupt settings...");
    x = getInterrupt(&thresholdTeslaX10002, &hysteresisTeslaX10002, &activeHigh2);
    tr_debug("Result of reading SI7210 is %d.", x);
    tr_debug("Interrupt threshold is %.3f.", ((float) thresholdTeslaX10002) / 1000);
    TEST_ASSERT(thresholdTeslaX10002 == 80);
    tr_debug("Hysteresis is %.3f.", ((float) hysteresisTeslaX10002) / 1000);
    TEST_ASSERT(hysteresisTeslaX10002 == 8960);
    tr_debug("Active high is %s.", activeHigh2 ? "true" : "false");
    TEST_ASSERT(activeHigh2 == activeHigh1);

    si7210Deinit();

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
    Case("Get field strength reading", test_reading),
    Case("Change range", test_range),
    Case("Interrupt setting", test_interrupt)
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
