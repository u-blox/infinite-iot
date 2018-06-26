#include "greentea-client/test_env.h"
#include "unity.h"
#include "utest.h"
#include "mbed_trace.h"
#include "mbed.h"
#include "act_voltages.h"
#include "eh_post.h"
#include "eh_processor.h"
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
// To run them, before you do "mbed test", you need
// to (once) do "mbedls --m 0004:UBLOX_EVK_NINA_B1" to
// set up the right target name, otherwise Mbed thinks
// you have an LPC2368 attached.
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

#if defined (TARGET_UBLOX_C030_U201)
// The expected desirability table after post() for
// TARGET_UBLOX_C030_U201: modem is present, and
// BME280, LIS3DH and ZOEM8 are expected to have
// been connected to I2C.
static const Desirability gExpectedDesirability[] = {0, /* ACTION_TYPE_NULL */
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
static const Desirability gExpectedDesirability[] = {0, /* ACTION_TYPE_NULL */
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

// Test of power-on self-test
void test_post() {
    mbed_stats_heap_t statsHeapBefore;
    mbed_stats_heap_t statsHeapAfter;
    Desirability d;

    tr_debug("Print something out as tr_debug seems to allocate from the heap when first called.\n");

    // Initialise things
    actionInit();
    processorInit();

    // Capture the heap stats before we start
    mbed_stats_heap_get(&statsHeapBefore);
    tr_debug("%d byte(s) of heap used at the outset.", (int) statsHeapBefore.current_size);

    // Check that we can initialise things, tolerating failures
    // since not everything might be attached in all cases
    TEST_ASSERT(post(true) == POST_RESULT_OK);

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

    // These two are normally left on so de-initialise them here
    si7210Deinit();
    lis3dhDeinit();

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
    Case("POST", test_post)
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
