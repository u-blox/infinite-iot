#include "greentea-client/test_env.h"
#include "unity.h"
#include "utest.h"
#include "mbed_trace.h"
#include "mbed.h"
#include "act_voltages.h"
#include "eh_data.h"
#include "eh_action.h"
#include "eh_processor.h"

using namespace utest::v1;

// These are tests for the eh_processor module.
//
// ----------------------------------------------------------------
// COMPILE-TIME MACROS
// ----------------------------------------------------------------

#define TRACE_GROUP "PROCESSOR"

// The wait time while in the doAction() thread loop
#define THREAD_ACTION_WAIT_TIME_MS 500

// ----------------------------------------------------------------
// PRIVATE VARIABLES
// ----------------------------------------------------------------

// Lock for debug prints
static Mutex gMtx;

// The action callback count
static int gActionCallbackCount[MAX_NUM_ACTION_TYPES];

// The bool to be returned by threadDiagosticsCallback()
static bool gKeepThreadGoing = true;

// An event queue for the processor
static EventQueue gWakeUpEventQueue(/* event count */ 10 * EVENTS_EVENT_SIZE);

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
static bool threadDiagosticsCallback(Action *pAction)
{
    gActionCallbackCount[pAction->type]++;
    wait_ms(THREAD_ACTION_WAIT_TIME_MS);

    return gKeepThreadGoing;
}

// ----------------------------------------------------------------
// TESTS
// ----------------------------------------------------------------

// Test of spinning up tasks to perform actions without terminating any
void test_tasking_no_termination() {
    mbed_stats_heap_t statsHeapBefore;
    mbed_stats_heap_t statsHeapAfter;
    Data *pData;
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

    // Now kick off a thread that runs processorHandleWakeup()
    // and, while it's running, set power to bad again
    // in order to make it exit
    memset(&gActionCallbackCount, 0, sizeof (gActionCallbackCount));
    gKeepThreadGoing = true;
    TEST_ASSERT(pProcessorThread->start(callback(processorHandleWakeup, &gWakeUpEventQueue)) == osOK);
    wait_ms(THREAD_ACTION_WAIT_TIME_MS);
    voltageFakeIsGood(false);
    voltageFakeIsBad(true);
    pProcessorThread->join();
    delete pProcessorThread;

    // Check that the thread diagnostic has been called once for each action type
    // but start from ACTION_TYPE_MEASURE_HUMIDITY, i.e skipping the two reporting
    // actions as they will have been moved to the end of the list and so won't be
    // called within MAX_NUM_SIMULTANEOUS_ACTIONS (can't run more than this without
    // terminating an action (by setting gKeepThreadGoing to false))
    for (unsigned int x = ACTION_TYPE_MEASURE_HUMIDITY;
         x < MAX_NUM_SIMULTANEOUS_ACTIONS + ACTION_TYPE_MEASURE_HUMIDITY; x++) {
        tr_debug("Action type %d was called %d time(s).\n", x, gActionCallbackCount[x]);
        TEST_ASSERT(gActionCallbackCount[x] > 0);
    }

    // Should be no actions outstanding
    TEST_ASSERT(actionCount() == 0);

    // Clear any data items that may have been created
    pData = pDataFirst();
    while (pData != NULL) {
        dataFree(&pData);
        pData = pDataNext();
    }
    TEST_ASSERT(dataCount() == 0);

    // Capture the heap stats once more
    mbed_stats_heap_get(&statsHeapAfter);
    tr_debug("%d byte(s) of heap used at the end.", (int) statsHeapAfter.current_size);

    // The heap used should be the same as at the start
    TEST_ASSERT(statsHeapBefore.current_size == statsHeapAfter.current_size);

    // Stop the fakery
    processorSetThreadDiagnosticsCallback(NULL);
    voltageFakeIsBad(false);
}

// Test of spinning up tasks to perform all actions, terminating actions to complete
void test_tasking_with_termination() {
    mbed_stats_heap_t statsHeapBefore;
    mbed_stats_heap_t statsHeapAfter;
    Thread *pProcessorThread;
    Data *pData;

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

    // Create at least one data item, otherwise the reporting action
    // won't be called
    pData = pDataAlloc(NULL, DATA_TYPE_WAKE_UP_REASON, 0, NULL);
    TEST_ASSERT(dataCount() == 1);

    // Kick off the thread that runs processorHandleWakeup()
    memset(&gActionCallbackCount, 0, sizeof (gActionCallbackCount));
    gKeepThreadGoing = false;
    TEST_ASSERT(pProcessorThread->start(callback(processorHandleWakeup, &gWakeUpEventQueue)) == osOK);
    // Let the actions start and terminate
    wait_ms(THREAD_ACTION_WAIT_TIME_MS);
    pProcessorThread->join();
    delete pProcessorThread;

    // Check that the thread diagnostic has been called once for each action type
    // except 1, which will have been deleted since we will execute "report and
    // get time" rather than just "report" at the outset
    //for (unsigned int x = ACTION_TYPE_GET_TIME_AND_REPORT; x < MAX_NUM_ACTION_TYPES; x++) {
    //    tr_debug("Action type %d was called %d time(s).\n", x, gActionCallbackCount[x]);
    //    TEST_ASSERT(gActionCallbackCount[x] > 0);
    //}

    // Should be no actions outstanding
    TEST_ASSERT(actionCount() == 0);

    // Clear all data items that have been created
    pData = pDataFirst();
    while (pData != NULL) {
        dataFree(&pData);
        pData = pDataNext();
    }
    TEST_ASSERT(dataCount() == 0);

    // Capture the heap stats once more
    mbed_stats_heap_get(&statsHeapAfter);
    tr_debug("%d byte(s) of heap used at the end.", (int) statsHeapAfter.current_size);

    // The heap used should be the same as at the start
    TEST_ASSERT(statsHeapBefore.current_size == statsHeapAfter.current_size);

    // Stop the fakery
    gKeepThreadGoing = true;
    processorSetThreadDiagnosticsCallback(NULL);
    voltageFakeIsGood(false);
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
    Case("Tasking without termination", test_tasking_no_termination),
    Case("Tasking with termination", test_tasking_with_termination)
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
