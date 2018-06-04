#include "greentea-client/test_env.h"
#include "unity.h"
#include "utest.h"
#include "mbed_trace.h"
#include "mbed.h"
#include "act_voltages.h"
#include "eh_processor.h"
#define TRACE_GROUP "PROCESSOR"

using namespace utest::v1;

// These are tests for the eh_processor module.
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

// The action callback count
static int gActionCallbackCount[MAX_NUM_ACTION_TYPES];

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

// For thread diagnostics
static void threadDiagosticsCallback(Action *pAction)
{
    gActionCallbackCount[pAction->type]++;
}

// ----------------------------------------------------------------
// TESTS
// ----------------------------------------------------------------

// Test of spinning up tasks to perform actions
void test_tasking() {
    mbed_stats_heap_t statsHeapBefore;
    mbed_stats_heap_t statsHeapAfter;
    Thread *pProcessorThread;

    tr_debug("Print something out as tr_debug seems to allocate from the heap when first called.\n");

    // Capture the heap stats before we start
    mbed_stats_heap_get(&statsHeapBefore);
    tr_debug("%d byte(s) of heap used at the outset.", (int) statsHeapBefore.current_size);

    pProcessorThread = new Thread();

    // Initialise things
    actionInit();
    processorInit();

    // Set the callback for thread diagnostics and fake that power is good
    processorSetThreadDiagnosticsCallback(&threadDiagosticsCallback);
    voltageFakeIsGood(true);

    // Now kick off a thread that will call the processor
    // and, while it's running, set power to bad again
    // in order to make it exit
    TEST_ASSERT(pProcessorThread->start(processorHandleWakeup) == osOK);
    wait_ms(1000);
    voltageFakeIsGood(false);
    voltageFakeIsBad(true);
    pProcessorThread->join();
    delete pProcessorThread;

    // Check that the thread diagnostic has been called twice for each action type
    // up to MAX_NUM_SIMULTANEOUS_ACTIONS (can't run more than this without
    // terminating an action first, which we can't do from here)
    for (unsigned int x = 0; x < MAX_NUM_SIMULTANEOUS_ACTIONS; x++) {
        TEST_ASSERT(gActionCallbackCount[x + 1] == 2); // +1 to avoid ACTION_TYPE_NULL
    }

    // Capture the heap stats once more
    mbed_stats_heap_get(&statsHeapAfter);
    tr_debug("%d byte(s) of heap used at the end.", (int) statsHeapAfter.current_size);

    // The heap used should be the same as at the start
    TEST_ASSERT(statsHeapBefore.current_size == statsHeapAfter.current_size);

    // Stop the fakery
    processorSetThreadDiagnosticsCallback(NULL);
    voltageFakeIsGood(false);
    voltageFakeIsBad(false);
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
    Case("Tasking", test_tasking)
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
