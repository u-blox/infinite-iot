#include "greentea-client/test_env.h"
#include "unity.h"
#include "utest.h"
#include "mbed_trace.h"
#include "mbed.h"
#include "eh_action.h"
#define TRACE_GROUP "ACTION"

using namespace utest::v1;

// These are tests for the eh_action module.
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

// Action list pointers
static Action *gpAction[MAX_NUM_ACTIONS];

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

// Test of adding actions
void test_add() {
    int actionType = ACTION_TYPE_NULL + 1;
    Action *pAction;
    int x = 0;

    initActions();

    // Fill up the action list with all action types except ACTION_TYPE_NULL
    for (pAction = pAddAction((ActionType) actionType); pAction != NULL; pAction = pAddAction((ActionType) actionType), x++) {
        gpAction[x] = pAction;
        actionType++;
        if (actionType >= MAX_NUM_ACTION_TYPES) {
            actionType = ACTION_TYPE_NULL + 1;
        }
    }
    TEST_ASSERT(x == MAX_NUM_ACTIONS);

    tr_debug("%d actions added.", x);

    // Check that the initial actions states are correct
    for (x = 0; x < MAX_NUM_ACTIONS; x++) {
        TEST_ASSERT(gpAction[x]->state == ACTION_STATE_REQUESTED);
    }

    // Set some of the actions to COMPLETED and check
    // that they are re-used
    tr_debug("Adding 2 more actions on top of COMPLETED ones.");
    TEST_ASSERT(gpAction[0]->type != ACTION_TYPE_NULL);
    TEST_ASSERT(gpAction[MAX_NUM_ACTIONS - 1]->type != ACTION_TYPE_NULL);
    gpAction[0]->state = ACTION_STATE_COMPLETED;
    gpAction[MAX_NUM_ACTIONS - 1]->state = ACTION_STATE_COMPLETED;
    gpAction[0] = pAddAction(ACTION_TYPE_NULL);
    TEST_ASSERT(gpAction[0] != NULL);
    TEST_ASSERT(gpAction[0]->state == ACTION_STATE_REQUESTED);
    TEST_ASSERT(gpAction[0]->type == ACTION_TYPE_NULL);
    gpAction[MAX_NUM_ACTIONS - 1] = pAddAction(ACTION_TYPE_NULL);
    TEST_ASSERT(gpAction[MAX_NUM_ACTIONS - 1] != NULL);
    TEST_ASSERT(gpAction[MAX_NUM_ACTIONS - 1]->state == ACTION_STATE_REQUESTED);
    TEST_ASSERT(gpAction[MAX_NUM_ACTIONS - 1]->type == ACTION_TYPE_NULL);
    TEST_ASSERT(pAddAction(ACTION_TYPE_NULL) == NULL);

    // Set some of the actions to ABORTED and check
    // that they are re-used
    tr_debug("Adding 2 more actions on top of ABORTED ones.");
    TEST_ASSERT(gpAction[0]->type == ACTION_TYPE_NULL);
    TEST_ASSERT(gpAction[MAX_NUM_ACTIONS - 1]->type == ACTION_TYPE_NULL);
    gpAction[0]->state = ACTION_STATE_ABORTED;
    gpAction[MAX_NUM_ACTIONS - 1]->state = ACTION_STATE_ABORTED;
    gpAction[0] = pAddAction((ActionType) (MAX_NUM_ACTION_TYPES - 1));
    TEST_ASSERT(gpAction[0] != NULL);
    TEST_ASSERT(gpAction[0]->state == ACTION_STATE_REQUESTED);
    TEST_ASSERT(gpAction[0]->type == MAX_NUM_ACTION_TYPES - 1);
    gpAction[MAX_NUM_ACTIONS - 1] = pAddAction((ActionType) (MAX_NUM_ACTION_TYPES - 1));
    TEST_ASSERT(gpAction[MAX_NUM_ACTIONS - 1] != NULL);
    TEST_ASSERT(gpAction[MAX_NUM_ACTIONS - 1]->state == ACTION_STATE_REQUESTED);
    TEST_ASSERT(gpAction[MAX_NUM_ACTIONS - 1]->type == MAX_NUM_ACTION_TYPES - 1);
    TEST_ASSERT(pAddAction(ACTION_TYPE_NULL) == NULL);
}

// Test of ranking actions
void test_rank_1() {
    int actionType = ACTION_TYPE_NULL + 1;
    int x = 0;
    int y = 0;
    time_t timeStamp = time(NULL);

    initActions();

    // Fill up the action list
    do {
        gpAction[x] = pAddAction((ActionType) actionType);
        x++;
        actionType++;
        if (actionType >= MAX_NUM_ACTION_TYPES) {
            actionType = ACTION_TYPE_NULL + 1;
        }
    } while (gpAction[x - 1] != NULL);
    x--;
    TEST_ASSERT(x == MAX_NUM_ACTIONS);

    tr_debug("%d actions added.", x);

    // Go through the action list in reverse order and assign times
    // that differ by 1 second in ascending order
    for (x = MAX_NUM_ACTIONS - 1; x >= 0; x--) {
        gpAction[x]->timeCompletedUTC = timeStamp;
        timeStamp++;
    }

    tr_debug("Ranking actions.");
    // Now rank the action types and get back the first
    // ranked action type
    actionType = rankActionTypes();

    // The action types should be ranked according to time, the
    // oldest first.  The oldest is the type at the end of
    // the action list
    for (x = MAX_NUM_ACTIONS - 1; (actionType != ACTION_TYPE_NULL) && (x >= 0); x--) {
        TEST_ASSERT(actionType == gpAction[x]->type);
        y++;
        actionType = nextActionType();
    }
    TEST_ASSERT(y == MAX_NUM_ACTION_TYPES - 1); // -1 to omit ACTION_TYPE_NULL
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
    Case("Add actions", test_add),
    Case("Rank actions", test_rank_1)
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
