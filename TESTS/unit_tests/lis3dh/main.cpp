#include "greentea-client/test_env.h"
#include "unity.h"
#include "utest.h"
#include "mbed_trace.h"
#include "mbed.h"
#include "eh_config.h"
#include "eh_i2c.h"
#include "act_lis3dh.h"
#include "act_acceleration.h"

using namespace utest::v1;

// These are tests for the act_lis3dh accelerometer driver.
//
// ----------------------------------------------------------------
// COMPILE-TIME MACROS
// ----------------------------------------------------------------

#define TRACE_GROUP "LIS3DH"

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

// Test of obtaining an acceleration reading
void test_reading() {
    int x = 0;
    int readingXGX1000 = 0;
    int readingYGX1000 = 0;
    int readingZGX1000 = 0;
    mbed_stats_heap_t statsHeapBefore;
    mbed_stats_heap_t statsHeapAfter;

    tr_debug("Print something out as tr_debug seems to allocate from the heap when first called.\n");

    // Capture the heap stats before we start
    mbed_stats_heap_get(&statsHeapBefore);
    tr_debug("%d byte(s) of heap used at the outset.", (int) statsHeapBefore.current_size);

    // Instantiate I2C
    i2cInit(I2C_DATA, I2C_CLOCK);

    // Try to take a reading before initialisation - should fail
    TEST_ASSERT(getAcceleration(&readingXGX1000, &readingYGX1000, &readingZGX1000) == ACTION_DRIVER_ERROR_NOT_INITIALISED);

    tr_debug("Initialising LIS3DH...");
    TEST_ASSERT(lis3dhInit(LIS3DH_ADDRESS) == ACTION_DRIVER_OK);

    // Get an acceleration reading
    tr_debug("Reading LIS3DH...");
    x = getAcceleration(&readingXGX1000, &readingYGX1000, &readingZGX1000);
    tr_debug("Result of reading LIS3DH is %d.", x);
    TEST_ASSERT(x == ACTION_DRIVER_OK);
    tr_debug("Acceleration is x: %d, y: %d, z: %d.", readingXGX1000, readingYGX1000, readingZGX1000);

    // Repeat leaving parameters as NULL in various combinations
    TEST_ASSERT(getAcceleration(&readingXGX1000, &readingYGX1000, NULL) == ACTION_DRIVER_OK)
    TEST_ASSERT(getAcceleration(&readingXGX1000, NULL, NULL) == ACTION_DRIVER_OK)
    TEST_ASSERT(getAcceleration(NULL, NULL, NULL) == ACTION_DRIVER_OK)
    TEST_ASSERT(getAcceleration(NULL, &readingYGX1000, &readingZGX1000) == ACTION_DRIVER_OK)
    TEST_ASSERT(getAcceleration(NULL, NULL, &readingZGX1000) == ACTION_DRIVER_OK)
    TEST_ASSERT(getAcceleration(NULL, &readingYGX1000, NULL) == ACTION_DRIVER_OK)
    TEST_ASSERT(getAcceleration(&readingXGX1000, NULL, &readingZGX1000) == ACTION_DRIVER_OK)

    lis3dhDeinit();

    // Shut down I2C
    i2cDeinit();

    // Capture the heap stats once more
    mbed_stats_heap_get(&statsHeapAfter);
    tr_debug("%d byte(s) of heap used at the end.", (int) statsHeapAfter.current_size);

    // The heap used should be the same as at the start
    TEST_ASSERT(statsHeapBefore.current_size == statsHeapAfter.current_size);
}

// Test of setting device sensitivity
void test_sensitivity() {
    int x = 0;
    unsigned char y;
    unsigned char sensitivity = 0;
    mbed_stats_heap_t statsHeapBefore;
    mbed_stats_heap_t statsHeapAfter;

    tr_debug("Print something out as tr_debug seems to allocate from the heap when first called.\n");

    // Capture the heap stats before we start
    mbed_stats_heap_get(&statsHeapBefore);
    tr_debug("%d byte(s) of heap used at the outset.", (int) statsHeapBefore.current_size);

    // Instantiate I2C
    i2cInit(I2C_DATA, I2C_CLOCK);

    // Try to get/set before initialisation - should fail
    TEST_ASSERT(lis3dhGetSensitivity(&sensitivity) == ACTION_DRIVER_ERROR_NOT_INITIALISED);
    TEST_ASSERT(lis3dhSetSensitivity(sensitivity) == ACTION_DRIVER_ERROR_NOT_INITIALISED);

    tr_debug("Initialising LIS3DH...");
    TEST_ASSERT(lis3dhInit(LIS3DH_ADDRESS) == ACTION_DRIVER_OK);

    for (y = 0; y < 4; y++) {
        tr_debug("Setting sensitivity of LIS3DH to %d...", y);
        x = lis3dhSetSensitivity(y);
        tr_debug("Result of setting sensitivity of LIS3DH is %d.", x);
        TEST_ASSERT(x == ACTION_DRIVER_OK);
        tr_debug("Reading sensitivity of LIS3DH...");
        x = lis3dhGetSensitivity(&sensitivity);
        tr_debug("Result of reading LIS3DH is %d.", x);
        TEST_ASSERT(x == ACTION_DRIVER_OK);
        tr_debug("Sensitivity is %d.", sensitivity);
        TEST_ASSERT(sensitivity == y);
    }

    // Check out of range
    tr_debug("Setting sensitivity of LIS3DH to %d...", y);
    TEST_ASSERT(lis3dhSetSensitivity(y) == ACTION_DRIVER_ERROR_PARAMETER);
    tr_debug("Reading sensitivity of LIS3DH...");
    TEST_ASSERT(lis3dhGetSensitivity(&sensitivity) == ACTION_DRIVER_OK);
    tr_debug("Sensitivity is %d.", sensitivity);
    TEST_ASSERT(sensitivity == 3);

    // Try with NULL parameter
    TEST_ASSERT(lis3dhGetSensitivity(NULL) == ACTION_DRIVER_OK)

    lis3dhDeinit();

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
    unsigned int thresholdMG1a;
    unsigned int thresholdMG1b;
    bool enabledNotDisabled1a = true;
    bool enabledNotDisabled1b = true;
    unsigned int thresholdMG2a;
    bool enabledNotDisabled2a;
    bool enabledNotDisabled2b;
    mbed_stats_heap_t statsHeapBefore;
    mbed_stats_heap_t statsHeapAfter;

    tr_debug("Print something out as tr_debug seems to allocate from the heap when first called.\n");

    // Capture the heap stats before we start
    mbed_stats_heap_get(&statsHeapBefore);
    tr_debug("%d byte(s) of heap used at the outset.", (int) statsHeapBefore.current_size);

    // Instantiate I2C
    i2cInit(I2C_DATA, I2C_CLOCK);

    // Try before initialisation - should fail
    TEST_ASSERT(lis3dhGetInterruptThreshold(1, &thresholdMG1a) == ACTION_DRIVER_ERROR_NOT_INITIALISED);
    TEST_ASSERT(lis3dhSetInterruptThreshold(1, thresholdMG1a) == ACTION_DRIVER_ERROR_NOT_INITIALISED);
    TEST_ASSERT(lis3dhSetInterruptEnable(1, enabledNotDisabled1a) == ACTION_DRIVER_ERROR_NOT_INITIALISED);
    TEST_ASSERT(lis3dhGetInterruptEnable(1, &enabledNotDisabled1a) == ACTION_DRIVER_ERROR_NOT_INITIALISED);

    tr_debug("Initialising LIS3DH...");
    TEST_ASSERT(lis3dhInit(LIS3DH_ADDRESS) == ACTION_DRIVER_OK);
    tr_debug("Set to defaults...");
    TEST_ASSERT(lis3dhSetSensitivity(0) == ACTION_DRIVER_OK);
    x = lis3dhSetInterruptEnable(1, false);
    tr_debug("Result of disabling interrupt 1 is %d.", x);
    TEST_ASSERT(x == ACTION_DRIVER_OK);
    x = lis3dhSetInterruptEnable(2, false);
    tr_debug("Result of disabling interrupt 2 is %d.", x);
    TEST_ASSERT(x == ACTION_DRIVER_OK);
    x = lis3dhSetInterruptThreshold(1, 0);
    tr_debug("Result of setting interrupt 1 threshold is %d.", x);
    TEST_ASSERT(x == ACTION_DRIVER_OK);
    x = lis3dhSetInterruptThreshold(2, 0);
    tr_debug("Result of setting interrupt 2 threshold is %d.", x);
    TEST_ASSERT(x == ACTION_DRIVER_OK);

    // Get interrupt 1 sensitivity
    tr_debug("Reading LIS3DH interrupt 1 threshold...");
    x = lis3dhGetInterruptThreshold(1, &thresholdMG1a);
    tr_debug("Result of reading interrupt 1 threshold is %d.", x);
    TEST_ASSERT(x == ACTION_DRIVER_OK);
    tr_debug("Interrupt 1 is threshold %d mG.", thresholdMG1a);

    // Increase it and write it back
    tr_debug("Writing LIS3DH interrupt 1 threshold...");
    x = lis3dhSetInterruptThreshold(1, thresholdMG1a + 200);
    tr_debug("Result of writing interrupt 1 threshold is %d.", x);
    TEST_ASSERT(x == ACTION_DRIVER_OK);

    // Check it
    tr_debug("Reading LIS3DH interrupt 1 threshold...");
    TEST_ASSERT(lis3dhGetInterruptThreshold(1, &thresholdMG1b) == ACTION_DRIVER_OK);
    tr_debug("Interrupt 1 threshold is %d mG.", thresholdMG1b);
    TEST_ASSERT(thresholdMG1b > thresholdMG1a);

    // Get interrupt 2 sensitivity and check that it's not been incremented
    tr_debug("Reading LIS3DH interrupt 2 threshold...");
    TEST_ASSERT(lis3dhGetInterruptThreshold(2, &thresholdMG2a) == ACTION_DRIVER_OK);
    tr_debug("Interrupt 2 threshold is %d mG.", thresholdMG2a);
    TEST_ASSERT(thresholdMG1b > thresholdMG2a);

    // Check for upper limit in range 0
    thresholdMG1a = 2100;
    tr_debug("Setting LIS3DH interrupt 1 threshold to max...");
    TEST_ASSERT(lis3dhSetInterruptThreshold(1, thresholdMG1a) == ACTION_DRIVER_OK);
    TEST_ASSERT(lis3dhGetInterruptThreshold(1, &thresholdMG1b) == ACTION_DRIVER_OK);
    tr_debug("Interrupt 1 threshold is %d mG.", thresholdMG1b);
    TEST_ASSERT(thresholdMG1b == 2032);

    // Check for upper limit in range 1
    thresholdMG1a = 4100;
    tr_debug("Set sensitivity to 1...");
    TEST_ASSERT(lis3dhSetSensitivity(1) == ACTION_DRIVER_OK);
    tr_debug("Setting LIS3DH interrupt 1 threshold to max...");
    TEST_ASSERT(lis3dhSetInterruptThreshold(1, thresholdMG1a) == ACTION_DRIVER_OK);
    TEST_ASSERT(lis3dhGetInterruptThreshold(1, &thresholdMG1b) == ACTION_DRIVER_OK);
    tr_debug("Interrupt 1 threshold is %d mG.", thresholdMG1b);
    TEST_ASSERT(thresholdMG1b == 4064);

    // Check for upper limit in range 2
    thresholdMG1a = 8200;
    tr_debug("Set sensitivity to 2...");
    TEST_ASSERT(lis3dhSetSensitivity(2) == ACTION_DRIVER_OK);
    tr_debug("Setting LIS3DH interrupt 1 threshold to max...");
    TEST_ASSERT(lis3dhSetInterruptThreshold(1, thresholdMG1a) == ACTION_DRIVER_OK);
    TEST_ASSERT(lis3dhGetInterruptThreshold(1, &thresholdMG1b) == ACTION_DRIVER_OK);
    tr_debug("Interrupt 1 threshold is %d mG.", thresholdMG1b);
    TEST_ASSERT(thresholdMG1b == 7874);

    // Check for upper limit in range 3
    thresholdMG1a = 16400;
    tr_debug("Set sensitivity to 3...");
    TEST_ASSERT(lis3dhSetSensitivity(3) == ACTION_DRIVER_OK);
    tr_debug("Setting LIS3DH interrupt 1 threshold to max...");
    TEST_ASSERT(lis3dhSetInterruptThreshold(1, thresholdMG1a) == ACTION_DRIVER_OK);
    TEST_ASSERT(lis3dhGetInterruptThreshold(1, &thresholdMG1b) == ACTION_DRIVER_OK);
    tr_debug("Interrupt 1 threshold is %d mG.", thresholdMG1b);
    TEST_ASSERT(thresholdMG1b == 16368);

    // Make sure enable and disable work
    TEST_ASSERT(lis3dhGetInterruptEnable(1, &enabledNotDisabled1a) ==  ACTION_DRIVER_OK);
    TEST_ASSERT(!enabledNotDisabled1a);
    TEST_ASSERT(lis3dhSetInterruptEnable(1, !enabledNotDisabled1a) == ACTION_DRIVER_OK);
    TEST_ASSERT(lis3dhGetInterruptEnable(1, &enabledNotDisabled1b) ==  ACTION_DRIVER_OK);
    TEST_ASSERT(enabledNotDisabled1b != enabledNotDisabled1a);
    TEST_ASSERT(lis3dhGetInterruptEnable(2, &enabledNotDisabled2a) ==  ACTION_DRIVER_OK);
    TEST_ASSERT(!enabledNotDisabled2a);
    TEST_ASSERT(lis3dhSetInterruptEnable(2, !enabledNotDisabled2a) == ACTION_DRIVER_OK);
    TEST_ASSERT(lis3dhGetInterruptEnable(2, &enabledNotDisabled2b) ==  ACTION_DRIVER_OK);
    TEST_ASSERT(enabledNotDisabled2b != enabledNotDisabled2a);

    // Make sure that clearing the interrupt works
    x = lis3dhClearInterrupt(1);
    TEST_ASSERT((x == ACTION_DRIVER_OK) || (x == ACTION_DRIVER_ERROR_NO_INTERRUPT));
    x = lis3dhClearInterrupt(2);
    TEST_ASSERT((x == ACTION_DRIVER_OK) || (x == ACTION_DRIVER_ERROR_NO_INTERRUPT));

    // Check for parameter errors
    TEST_ASSERT(lis3dhSetInterruptThreshold(0, thresholdMG1a) == ACTION_DRIVER_ERROR_PARAMETER);
    TEST_ASSERT(lis3dhGetInterruptThreshold(0, &thresholdMG1a) == ACTION_DRIVER_ERROR_PARAMETER);
    TEST_ASSERT(lis3dhSetInterruptEnable(0, enabledNotDisabled1a) == ACTION_DRIVER_ERROR_PARAMETER);
    TEST_ASSERT(lis3dhGetInterruptEnable(0, &enabledNotDisabled1b) ==  ACTION_DRIVER_ERROR_PARAMETER);
    TEST_ASSERT(lis3dhClearInterrupt(0) == ACTION_DRIVER_ERROR_PARAMETER);
    TEST_ASSERT(lis3dhSetInterruptThreshold(3, thresholdMG1a) == ACTION_DRIVER_ERROR_PARAMETER);
    TEST_ASSERT(lis3dhGetInterruptThreshold(3, &thresholdMG1a) == ACTION_DRIVER_ERROR_PARAMETER);
    TEST_ASSERT(lis3dhSetInterruptEnable(3, enabledNotDisabled1a) == ACTION_DRIVER_ERROR_PARAMETER);
    TEST_ASSERT(lis3dhGetInterruptEnable(3, &enabledNotDisabled1b) ==  ACTION_DRIVER_ERROR_PARAMETER);
    TEST_ASSERT(lis3dhClearInterrupt(3) == ACTION_DRIVER_ERROR_PARAMETER);

    // Try with NULL parameter
    TEST_ASSERT(lis3dhGetInterruptThreshold(1, NULL) == ACTION_DRIVER_OK);
    TEST_ASSERT(lis3dhGetInterruptEnable(1, NULL) ==  ACTION_DRIVER_OK);

    lis3dhDeinit();

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
    Case("Get acceleration", test_reading),
    Case("Sensitivity setting", test_sensitivity),
    Case("Interrupt setting", test_interrupt)
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
