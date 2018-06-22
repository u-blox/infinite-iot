#include "greentea-client/test_env.h"
#include "unity.h"
#include "utest.h"
#include "mbed_trace.h"
#include "mbed.h"
#include "eh_config.h" // for APN, USERNAME and PASSWORD
#include "eh_utilities.h" // for ARRAY_SIZE
#include "act_modem.h"
#define TRACE_GROUP "MODEM"

using namespace utest::v1;

// These are tests for the eh_modem module.
// To run them, before you do "mbed test", you need
// to (once) do "mbedls --m 0004:UBLOX_EVK_NINA_B1" to
// set up the right target name, otherwise Mbed thinks
// you have an LPC2368 attached.
//
// ----------------------------------------------------------------
// COMPILE-TIME MACROS
// ----------------------------------------------------------------

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

// Test initialisation
void test_init() {
    mbed_stats_heap_t statsHeapBefore;
    mbed_stats_heap_t statsHeapAfter;

    tr_debug("Print something out as tr_debug seems to allocate from the heap when first called.\n");

    // Capture the heap stats before we start
    mbed_stats_heap_get(&statsHeapBefore);
    tr_debug("%d byte(s) of heap used at the outset.", (int) statsHeapBefore.current_size);

    // Initialise the modem
    TEST_ASSERT(modemInit(SIM_PIN, APN, USERNAME, PASSWORD) == ACTION_DRIVER_OK)
    modemDeinit();

    // Now do it again: should be quicker this timer around as the modem
    // module will remember what sort of modem is attached
    TEST_ASSERT(modemInit(SIM_PIN, APN, USERNAME, PASSWORD) == ACTION_DRIVER_OK)
    modemDeinit();

    // Capture the heap stats once more
    mbed_stats_heap_get(&statsHeapAfter);
    tr_debug("%d byte(s) of heap used at the end.", (int) statsHeapAfter.current_size);

    // The heap used should be the same as at the start
    TEST_ASSERT(statsHeapBefore.current_size == statsHeapAfter.current_size);
}

// Test getting the IMEI
void test_get_imei() {
    mbed_stats_heap_t statsHeapBefore;
    mbed_stats_heap_t statsHeapAfter;
    char buf[MODEM_IMEI_LENGTH];

    tr_debug("Print something out as tr_debug seems to allocate from the heap when first called.\n");

    // Capture the heap stats before we start
    mbed_stats_heap_get(&statsHeapBefore);
    tr_debug("%d byte(s) of heap used at the outset.", (int) statsHeapBefore.current_size);

    // Ask for the IMEI before the modem is initialised: should fail
    TEST_ASSERT(modemGetImei(buf) == ACTION_DRIVER_ERROR_NOT_INITIALISED);

    // Initialise the modem
    TEST_ASSERT(modemInit(SIM_PIN, APN, USERNAME, PASSWORD) == ACTION_DRIVER_OK);

    // Ask for the IMEI again
    TEST_ASSERT(modemGetImei(buf) == ACTION_DRIVER_OK);
    tr_debug("IMEI: %s.", buf);
    TEST_ASSERT(strlen(buf) == 15);

    // Ask with the parameter NULL
    TEST_ASSERT(modemGetImei(NULL) == ACTION_DRIVER_OK);

    modemDeinit();

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
    Case("Initialisation", test_init),
    Case("Get IMEI", test_get_imei)
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
