#include "greentea-client/test_env.h"
#include "unity.h"
#include "utest.h"
#include "mbed_trace.h"
#include "mbed.h"
#include "eh_config.h"
#include "eh_i2c.h"
#include "act_si1133.h"
#include "act_light.h"
#define TRACE_GROUP "PROCESSOR"

using namespace utest::v1;

// These are tests for the act_si1133 light and UV
// sensor driver.
// To run them, before you do "mbed test", you need
// to (once) do "mbedls --m 0004:UBLOX_EVK_NINA_B1" to
// set up the right target name, otherwise Mbed thinks
// you have an LPC2368 attached.
//
// ----------------------------------------------------------------
// COMPILE-TIME MACROS
// ----------------------------------------------------------------

// The I2C address of the SI1133
#ifdef TARGET_TB_SENSE_12
// On the Thunderboard 2 pin AD is pulled to VDD
# define SI1133_ADDRESS SI1133_DEFAULT_ADDRESS_AD_VDD
#else
# define SI1133_ADDRESS SI1133_DEFAULT_ADDRESS
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
    i2cInit(PIN_I2C_SDA, PIN_I2C_SCL);

    tr_debug("Initialising SI1133...");
    x = si1133Init(SI1133_ADDRESS);
    tr_debug("Result of initialising SI1133 was %d.", x);
    TEST_ASSERT(x == ACTION_DRIVER_OK);
    si1133Deinit();

    // Shut down I2C
    i2cDeinit();

    // Capture the heap stats once more
    mbed_stats_heap_get(&statsHeapAfter);
    tr_debug("%d byte(s) of heap used at the end.", (int) statsHeapAfter.current_size);

    // The heap used should be the same as at the start
    TEST_ASSERT(statsHeapBefore.current_size == statsHeapAfter.current_size);
}

// Test of obtaining readings
void test_reading() {
    int x = 0;
    int lux;
    int uvIndexX1000;
    mbed_stats_heap_t statsHeapBefore;
    mbed_stats_heap_t statsHeapAfter;

    tr_debug("Print something out with a float (%f) in it as tr_debug and floats allocate from the heap when first called.\n", 1.0);

    // Capture the heap stats before we start
    mbed_stats_heap_get(&statsHeapBefore);
    tr_debug("%d byte(s) of heap used at the outset.", (int) statsHeapBefore.current_size);

    // Instantiate I2C
    i2cInit(PIN_I2C_SDA, PIN_I2C_SCL);

    // Try to get a reading before initialisation - should fail
    TEST_ASSERT(getLight(&lux, &uvIndexX1000) == ACTION_DRIVER_ERROR_NOT_INITIALISED)

    tr_debug("Initialising SI1133...");
    TEST_ASSERT(si1133Init(SI1133_ADDRESS) == ACTION_DRIVER_OK);

    // Get a reading of both lux and UV index
    tr_debug("Reading SI1133...");
    x = getLight(&lux, &uvIndexX1000);
    tr_debug("Result of reading SI1133 is %d.", x);
    TEST_ASSERT(x == ACTION_DRIVER_OK);
    tr_debug("Lux %d, UV index %.3f.", lux, ((float) uvIndexX1000) / 1000);
    // Again, but miss out uvIndex
    x = getLight(&lux, NULL);
    tr_debug("Result of reading SI1133 is %d.", x);
    TEST_ASSERT(x == ACTION_DRIVER_OK);
    tr_debug("Lux %d.", lux);
    // Again, but miss out lux
    x = getLight(NULL, &uvIndexX1000);
    tr_debug("Result of reading SI1133 is %d.", x);
    TEST_ASSERT(x == ACTION_DRIVER_OK);
    tr_debug("UV index %.3f.", ((float) uvIndexX1000) / 1000);
    // Again, but miss out both
    x = getLight(NULL, NULL);
    tr_debug("Result of reading SI1133 is %d.", x);
    TEST_ASSERT(x == ACTION_DRIVER_OK);

    si1133Deinit();

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
    Case("Get UV/light readings", test_reading)
};

Specification specification(test_setup, cases);

// ----------------------------------------------------------------
// MAIN
// ----------------------------------------------------------------

int main()
{

#ifdef TARGET_TB_SENSE_12
    // If testing on the Thunderboard 2, need to power the sensor up
    // by setting PF9 high
    DigitalOut enableSi1133(PF9, 1);
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
