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

// Set the interrupt and check the get result.
static void interrupt(unsigned int setThreshold,
                      unsigned int setHysteresis,
                      bool setActiveHigh,
                      unsigned int getThreshold,
                      unsigned int getHysteresis,
                      bool getActiveHigh)
{
    int x;
    unsigned int threshold;
    unsigned int hysteresis;
    bool activeHigh;

    // Set the interrupt settings
    tr_debug("Set interrupt settings with threshold %d," \
             " hysteresis %d, active high %s...", setThreshold,
             setHysteresis, setActiveHigh ? "true" : "false");
    x = si7210SetInterrupt(setThreshold, setHysteresis, setActiveHigh);
    tr_debug("Result of setting SI7210 is %d.", x);
    TEST_ASSERT(x == ACTION_DRIVER_OK);
    // Read the interrupt settings again
    tr_debug("Get interrupt settings...");
    x = si7210GetInterrupt(&threshold, &hysteresis, &activeHigh);
    tr_debug("Result of reading SI7210 is %d.", x);
    TEST_ASSERT(x == ACTION_DRIVER_OK);
    tr_debug("Interrupt threshold is %.3f.", ((float) threshold) / 1000);
    TEST_ASSERT(threshold == getThreshold);
    tr_debug("Hysteresis is %.3f.", ((float) hysteresis) / 1000);
    TEST_ASSERT(hysteresis == getHysteresis);
    tr_debug("Active high is %s.", activeHigh ? "true" : "false");
    TEST_ASSERT(activeHigh == getActiveHigh);
}

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
    TEST_ASSERT(si7210SetRange(SI7210_RANGE_20_MILLI_TESLAS) == ACTION_DRIVER_ERROR_NOT_INITIALISED)
    TEST_ASSERT(si7210SetRange(SI7210_RANGE_200_MILLI_TESLAS) == ACTION_DRIVER_ERROR_NOT_INITIALISED)

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
    x = si7210SetRange(SI7210_RANGE_200_MILLI_TESLAS);
    tr_debug("Result of changing range is %d.", x);
    TEST_ASSERT(x == ACTION_DRIVER_OK);
    TEST_ASSERT(si7210GetRange() == SI7210_RANGE_200_MILLI_TESLAS);

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
    x = si7210SetRange(SI7210_RANGE_20_MILLI_TESLAS);
    tr_debug("Result of changing range is %d.", x);
    TEST_ASSERT(x == ACTION_DRIVER_OK);
    TEST_ASSERT(si7210GetRange() == SI7210_RANGE_20_MILLI_TESLAS);

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
    unsigned int thresholdTeslaX1000 = 0;
    unsigned int hysteresisTeslaX1000 = 0;
    bool activeHigh = false;
    unsigned int getThresholdTeslaX1000;
    unsigned int getHysteresisTeslaX1000;
    bool getActiveHigh;
    mbed_stats_heap_t statsHeapBefore;
    mbed_stats_heap_t statsHeapAfter;

    tr_debug("Print something out with a float (%f) in it as tr_debug and floats allocate from the heap when first called.\n", 1.0);

    // Capture the heap stats before we start
    mbed_stats_heap_get(&statsHeapBefore);
    tr_debug("%d byte(s) of heap used at the outset.", (int) statsHeapBefore.current_size);

    // Instantiate I2C
    i2cInit(I2C_DATA, I2C_CLOCK);

    // Try to set up the interupt before initialisation - should fail
    TEST_ASSERT(si7210SetInterrupt(thresholdTeslaX1000, hysteresisTeslaX1000, activeHigh) == ACTION_DRIVER_ERROR_NOT_INITIALISED)
    TEST_ASSERT(si7210GetInterrupt(&thresholdTeslaX1000, &hysteresisTeslaX1000, &activeHigh) == ACTION_DRIVER_ERROR_NOT_INITIALISED)

    tr_debug("Initialising SI7210...");
    TEST_ASSERT(si7210Init(SI7210_ADDRESS) == ACTION_DRIVER_OK);

    // Get the current interrupt settings
    tr_debug("Get interrupt settings...");
    x = si7210GetInterrupt(&thresholdTeslaX1000, &hysteresisTeslaX1000, &activeHigh);
    tr_debug("Result of reading SI7210 is %d.", x);
    TEST_ASSERT(x == ACTION_DRIVER_OK);
    tr_debug("Interrupt threshold is %.3f.", ((float) thresholdTeslaX1000) / 1000);
    tr_debug("Hysteresis is %.3f.", ((float) hysteresisTeslaX1000) / 1000);
    tr_debug("Active high is %s.", activeHigh ? "true" : "false");

    // From the si7210 data sheet:
    // Threshold can be 0 or 80 to 19200 for the 20 milli-Tesla range (x10 for 200 milli-Tesla range)
    // If threshold is 0 then Hysteresis can be 80 to 17920 for the 20 milli-Tesla range (x10 for 200 milli-Tesla range)
    // else Hysteresis can be 40 to 8960 for the 20 milli-Tesla range (x10 for 200 milli-Tesla range)
    // each unit is 5 milli-Teslas (x10 for 200 milli-Tesla range)

    // Try with a threshold of 0, and the hysteresis below its usual minimum of 80, first
    tr_debug("Test limits in 20 milli-Tesla range");
    activeHigh = (rand() % 2 > 0) ? true : false;
    interrupt(0, 76, activeHigh, 0, 80, activeHigh);
    // Try again with threshold of 0 and hysteresis above its maximum of 17920
    activeHigh = (rand() % 2 > 0) ? true : false;
    interrupt(0, 17926, activeHigh, 0, 17920, activeHigh);
    // Try with a threshold above its maximum of 19200 and the hysteresis above its usual minimum of 40
    activeHigh = (rand() % 2 > 0) ? true : false;
    interrupt(19204, 36, activeHigh, 19200, 40, activeHigh);
    // Try with a threshold below its usual minimum of 80 and the hysteresis above its maximum of 8960
    activeHigh = (rand() % 2 > 0) ? true : false;
    interrupt(76, 8964, activeHigh, 80, 8960, activeHigh);
    // Try with a threshold at its usual minimum of 80 and hysteresis at the magic value of 0
    activeHigh = (rand() % 2 > 0) ? true : false;
    interrupt(80, 0, activeHigh, 80, 0, activeHigh);

    // Switch to 200 milli-Tesla range and repeat the limit checks
    tr_debug("Test limits in 200 milli-Tesla range");
    TEST_ASSERT(si7210SetRange(SI7210_RANGE_200_MILLI_TESLAS) == ACTION_DRIVER_OK);
    activeHigh = (rand() % 2 > 0) ? true : false;
    interrupt(0, 751, activeHigh, 0, 800, activeHigh);
    activeHigh = (rand() % 2 > 0) ? true : false;
    interrupt(0, 179249, activeHigh, 0, 179200, activeHigh);
    activeHigh = (rand() % 2 > 0) ? true : false;
    interrupt(192049, 449, activeHigh, 192000, 400, activeHigh);
    activeHigh = (rand() % 2 > 0) ? true : false;
    interrupt(751, 89649, activeHigh, 800, 89600, activeHigh);

    // Try random values in-between in the 20 milli-Tesla range
    TEST_ASSERT(si7210SetRange(SI7210_RANGE_20_MILLI_TESLAS) == 0);
    tr_debug("Test random values");
    for (x = 0; x < 100; x++) {
        thresholdTeslaX1000 = (rand() % (19200 - 80) + 80);
        hysteresisTeslaX1000 = (rand() % (8960 - 40) + 40);
        activeHigh = (rand() % 2 > 0) ? true : false;
        tr_debug("Set interrupt settings with threshold %d," \
                 " hysteresis %d, active high %s...", thresholdTeslaX1000,
                 hysteresisTeslaX1000, activeHigh ? "true" : "false");
        TEST_ASSERT(si7210SetInterrupt(thresholdTeslaX1000, hysteresisTeslaX1000, activeHigh) == ACTION_DRIVER_OK);
        // Read the interrupt settings again
        tr_debug("Get interrupt settings...");
        TEST_ASSERT(si7210GetInterrupt(&getThresholdTeslaX1000, &getHysteresisTeslaX1000, &getActiveHigh) == ACTION_DRIVER_OK);
        tr_debug("Interrupt threshold is %.3f.", ((float) getThresholdTeslaX1000) / 1000);
        TEST_ASSERT_INT32_WITHIN(thresholdTeslaX1000 / 5, thresholdTeslaX1000, getThresholdTeslaX1000);
        tr_debug("Hysteresis is %.3f.", ((float) getHysteresisTeslaX1000) / 1000);
        TEST_ASSERT_INT32_WITHIN(hysteresisTeslaX1000 / 5, hysteresisTeslaX1000, getHysteresisTeslaX1000);
        tr_debug("Active high is %s.", getActiveHigh ? "true" : "false");
        TEST_ASSERT(activeHigh == getActiveHigh);
    }

    // Test get with NULL values
    getThresholdTeslaX1000 = 0;
    getHysteresisTeslaX1000 = 0;
    TEST_ASSERT(si7210GetInterrupt(&getThresholdTeslaX1000, &getHysteresisTeslaX1000, NULL) == ACTION_DRIVER_OK);
    TEST_ASSERT_INT32_WITHIN(thresholdTeslaX1000 / 5, thresholdTeslaX1000, getThresholdTeslaX1000);
    TEST_ASSERT_INT32_WITHIN(hysteresisTeslaX1000 / 5, hysteresisTeslaX1000, getHysteresisTeslaX1000);
    getThresholdTeslaX1000 = 0;
    getActiveHigh = 0;
    TEST_ASSERT(si7210GetInterrupt(&getThresholdTeslaX1000, NULL, &getActiveHigh) == ACTION_DRIVER_OK);
    TEST_ASSERT_INT32_WITHIN(thresholdTeslaX1000 / 5, thresholdTeslaX1000, getThresholdTeslaX1000);
    TEST_ASSERT(activeHigh == getActiveHigh);
    getHysteresisTeslaX1000 = 0;
    getActiveHigh = 0;
    TEST_ASSERT(si7210GetInterrupt(NULL, &getHysteresisTeslaX1000, &getActiveHigh) == ACTION_DRIVER_OK);
    TEST_ASSERT_INT32_WITHIN(hysteresisTeslaX1000 / 5, hysteresisTeslaX1000, getHysteresisTeslaX1000);
    TEST_ASSERT(activeHigh == getActiveHigh);

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
