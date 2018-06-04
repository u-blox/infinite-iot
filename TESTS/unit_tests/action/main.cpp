#include "greentea-client/test_env.h"
#include "unity.h"
#include "utest.h"
#include "mbed_trace.h"
#include "mbed.h"
#include "eh_action.h"
#include "eh_data.h"
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

// Add a data value that matters to the difference calculation
// (see dataDifference() in eh_data.cpp) to a data item.
static void addData(Action *pAction, int value)
{
    DataContents contents;

    memset (&contents, 0, sizeof(contents));

    switch (pAction->type) {
        case ACTION_TYPE_REPORT:
            // Deliberate fall-through
        case ACTION_TYPE_GET_TIME_AND_REPORT:
            // Report will include cellular data
            // and it's the rsrpDbm that matters to differencing
            contents.cellular.rsrpDbm = value;
            pAction->pData = pDataAlloc(pAction, DATA_TYPE_CELLULAR, 0, &contents);
        break;
        case ACTION_TYPE_MEASURE_HUMIDITY:
            contents.humidity.percentage = value;
            pAction->pData = pDataAlloc(pAction, DATA_TYPE_HUMIDITY, 0, &contents);
        break;
        case ACTION_TYPE_MEASURE_ATMOSPHERIC_PRESSURE:
            contents.atmosphericPressure.pascal = value;
            pAction->pData = pDataAlloc(pAction, DATA_TYPE_ATMOSPHERIC_PRESSURE, 0, &contents);
        break;
        case ACTION_TYPE_MEASURE_TEMPERATURE:
            contents.temperature.c = value;
            pAction->pData = pDataAlloc(pAction, DATA_TYPE_TEMPERATURE, 0, &contents);
        break;
        case ACTION_TYPE_MEASURE_LIGHT:
            // For light, the sum of lux and UV index values affect
            // variability but here we need to make sure that the value
            // only has a single (not multiple) effect, so just chose
            // light
            contents.light.lux = value;
            pAction->pData = pDataAlloc(pAction, DATA_TYPE_LIGHT, 0, &contents);
        break;
        case ACTION_TYPE_MEASURE_ORIENTATION:
            // For orientation, x, y and z all affect variability
            // here we chose just x for the reasons given above.
            contents.orientation.x = value;
            pAction->pData = pDataAlloc(pAction, DATA_TYPE_ORIENTATION, 0, &contents);
        break;
        case ACTION_TYPE_MEASURE_POSITION:
            // For position, all values have an effect, here we use
            // radiusMetres.
            contents.position.radiusMetres = value;
            pAction->pData = pDataAlloc(pAction, DATA_TYPE_POSITION, 0, &contents);
        break;
        case ACTION_TYPE_MEASURE_MAGNETIC:
            contents.magnetic.microTesla = value;
            pAction->pData = pDataAlloc(pAction, DATA_TYPE_MAGNETIC, 0, &contents);
        break;
        case ACTION_TYPE_MEASURE_BLE:
            // For BLE, x, y and z and batteryPercentage all affect variability
            // here we chose batteryPercentage.
            contents.ble.batteryPercentage = value;
            pAction->pData = pDataAlloc(pAction, DATA_TYPE_BLE, 0, &contents);
        break;
        default:
            MBED_ASSERT(false);
        break;
    }

    MBED_ASSERT(pAction->pData != NULL);
}

// Free any data attached to any of the actions
static void freeData()
{
    for (int x = 0; x < MAX_NUM_ACTIONS; x++) {
        if (gpAction[x]->pData != NULL) {
            dataFree((Data **) &(gpAction[x]->pData));
        }
    }
}

// ----------------------------------------------------------------
// TESTS
// ----------------------------------------------------------------

// Test that actions are included at start of day
void test_initial_actions() {
    int actionType = ACTION_TYPE_NULL + 1;
    Action *pAction;
    int x = 0;
    int y = 0;

    actionInit();

    // Set up the desirability for each action type (apart from the NULL one),
    // with the lower action types being least desirable
    for (x = ACTION_TYPE_NULL + 1; x < MAX_NUM_ACTION_TYPES; x++) {
        TEST_ASSERT(actionSetDesirability((ActionType) x, DESIRABILITY_DEFAULT + y));
        y++;
    }

    tr_debug("Looking for initial actions.");
    // Now rank the action types and get back the first
    // ranked action type
    actionType = actionRankTypes();

    // The action types should be there, ranked according to desirability, the
    // most desirable (highest number) first.
    y = MAX_NUM_ACTION_TYPES - 1;
    for (x = ACTION_TYPE_NULL + 1; (actionType != ACTION_TYPE_NULL) && (x < MAX_NUM_ACTION_TYPES); x++) {
        TEST_ASSERT(actionType == (ActionType) y);
        y--;
        actionType = actionNextType();
    }

    TEST_ASSERT(y == ACTION_TYPE_NULL);

    // Reset desirability to all defaults for the next test
    for (x = ACTION_TYPE_NULL + 1; x < MAX_NUM_ACTION_TYPES; x++) {
        TEST_ASSERT(actionSetDesirability((ActionType) x, DESIRABILITY_DEFAULT));
    }
}

// Test of adding actions
void test_add() {
    int actionType = ACTION_TYPE_NULL + 1;
    Action *pAction;
    int x = 0;

    actionInit();

    // Fill up the action list with all action types except ACTION_TYPE_NULL
    for (pAction = pActionAdd((ActionType) actionType); pAction != NULL; pAction = pActionAdd((ActionType) actionType), x++) {
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
    gpAction[0] = pActionAdd(ACTION_TYPE_NULL);
    TEST_ASSERT(gpAction[0] != NULL);
    TEST_ASSERT(gpAction[0]->state == ACTION_STATE_REQUESTED);
    TEST_ASSERT(gpAction[0]->type == ACTION_TYPE_NULL);
    gpAction[MAX_NUM_ACTIONS - 1] = pActionAdd(ACTION_TYPE_NULL);
    TEST_ASSERT(gpAction[MAX_NUM_ACTIONS - 1] != NULL);
    TEST_ASSERT(gpAction[MAX_NUM_ACTIONS - 1]->state == ACTION_STATE_REQUESTED);
    TEST_ASSERT(gpAction[MAX_NUM_ACTIONS - 1]->type == ACTION_TYPE_NULL);
    TEST_ASSERT(pActionAdd(ACTION_TYPE_NULL) == NULL);

    // Set some of the actions to ABORTED and check
    // that they are re-used
    tr_debug("Adding 2 more actions on top of ABORTED ones.");
    TEST_ASSERT(gpAction[0]->type == ACTION_TYPE_NULL);
    TEST_ASSERT(gpAction[MAX_NUM_ACTIONS - 1]->type == ACTION_TYPE_NULL);
    gpAction[0]->state = ACTION_STATE_ABORTED;
    gpAction[MAX_NUM_ACTIONS - 1]->state = ACTION_STATE_ABORTED;
    gpAction[0] = pActionAdd((ActionType) (MAX_NUM_ACTION_TYPES - 1));
    TEST_ASSERT(gpAction[0] != NULL);
    TEST_ASSERT(gpAction[0]->state == ACTION_STATE_REQUESTED);
    TEST_ASSERT(gpAction[0]->type == MAX_NUM_ACTION_TYPES - 1);
    gpAction[MAX_NUM_ACTIONS - 1] = pActionAdd((ActionType) (MAX_NUM_ACTION_TYPES - 1));
    TEST_ASSERT(gpAction[MAX_NUM_ACTIONS - 1] != NULL);
    TEST_ASSERT(gpAction[MAX_NUM_ACTIONS - 1]->state == ACTION_STATE_REQUESTED);
    TEST_ASSERT(gpAction[MAX_NUM_ACTIONS - 1]->type == MAX_NUM_ACTION_TYPES - 1);
    TEST_ASSERT(pActionAdd(ACTION_TYPE_NULL) == NULL);
}

// Test of ranking actions by time completed
void test_rank_time() {
    int actionType = ACTION_TYPE_NULL + 1;
    Action *pAction;
    int x = 0;
    int y = 0;
    time_t timeStamp = time(NULL);

    actionInit();

    // Fill up the action list
    for (pAction = pActionAdd((ActionType) actionType); pAction != NULL; pAction = pActionAdd((ActionType) actionType), x++) {
        gpAction[x] = pAction;
        actionType++;
        if (actionType >= MAX_NUM_ACTION_TYPES) {
            actionType = ACTION_TYPE_NULL + 1;
        }
    }
    TEST_ASSERT(x == MAX_NUM_ACTIONS);

    tr_debug("%d actions added.", x);

    // Go through the action list in reverse order and assign times
    // that differ by 1 second in ascending order
    for (x = MAX_NUM_ACTIONS - 1; x >= 0; x--) {
        gpAction[x]->timeCompletedUTC = timeStamp;
        timeStamp++;
    }

    tr_debug("Ranking actions by time, most recent first.");
    // Now rank the action types and get back the first
    // ranked action type
    actionType = actionRankTypes();

    // The action types should be ranked according to time, the
    // oldest first.  The oldest is the type at the end of
    // the action list
    for (x = MAX_NUM_ACTIONS - 1; (actionType != ACTION_TYPE_NULL) && (x >= 0); x--) {
        TEST_ASSERT(actionType == gpAction[x]->type);
        y++;
        actionType = actionNextType();
    }
    TEST_ASSERT(y == MAX_NUM_ACTION_TYPES - 1); // -1 to omit ACTION_TYPE_NULL
}

// Test of ranking actions by rarity
void test_rank_rarity() {
    int actionType;
    int lastActionType = ACTION_TYPE_NULL + 1;
    Action *pAction;
    int x = 0;
    int y;
    time_t timeStamp = time(NULL);

    actionInit();

    // Fill up the action list with MAX_NUM_ACTION_TYPES of the first action type, MAX_NUM_ACTION_TYPES - 1 of the second, etc.
    for (actionType = ACTION_TYPE_NULL + 1, x = MAX_NUM_ACTION_TYPES;
         (actionType < MAX_NUM_ACTION_TYPES) && (x > ACTION_TYPE_NULL) && (pAction != NULL);
         actionType++, x--) {
        for (y = x; (y > 0) && (pAction != NULL); y--) {
            pAction = pActionAdd((ActionType) actionType);
        }
    }

    // On completion of this process there may not have been room in the action list to
    // accommodate all of the numbers of types, so remember where we got to
   MBED_ASSERT(actionType > 0);
    if (pAction == NULL) {
        lastActionType = actionType - 1;
    }

    // Set the desirability of any of these missing actions to 0 to stop them
    // being added back into the list by the ranking process
    for (x = lastActionType + 1; x < MAX_NUM_ACTION_TYPES; x++) {
        TEST_ASSERT(actionSetDesirability((ActionType) x, 0));
    }

    tr_debug("Ranking actions by rarity, most rare first.");
    // Now rank the action types and get back the first
    // ranked action type
    actionType = actionRankTypes();

    // The action types we were able to add should be ranked according to
    // rarity
    for (x = lastActionType; (actionType != ACTION_TYPE_NULL); x--) {
        TEST_ASSERT(actionType == x);
        actionType = actionNextType();
    }

    // Reset desirability to all defaults for the next test
    for (x = ACTION_TYPE_NULL + 1; x < MAX_NUM_ACTION_TYPES; x++) {
        TEST_ASSERT(actionSetDesirability((ActionType) x, DESIRABILITY_DEFAULT));
    }
}

// Test of ranking actions by energy cost
void test_rank_energy() {
    int actionType = ACTION_TYPE_NULL + 1;
    Action *pAction;
    int x = 0;
    int y = 0;
    unsigned int energy = 0;

    actionInit();

    // Fill up the action list
    for (pAction = pActionAdd((ActionType) actionType); pAction != NULL; pAction = pActionAdd((ActionType) actionType), x++) {
        gpAction[x] = pAction;
        actionType++;
        if (actionType >= MAX_NUM_ACTION_TYPES) {
            actionType = ACTION_TYPE_NULL + 1;
        }
    }
    TEST_ASSERT(x == MAX_NUM_ACTIONS);

    tr_debug("%d actions added.", x);

    // Go through the action list in reverse order and assign energy values
    // that differ by 1 in ascending order
    for (x = MAX_NUM_ACTIONS - 1; x >= 0; x--) {
        gpAction[x]->energyCostUWH = energy;
        energy++;
    }

    tr_debug("Ranking actions by energy, smallest first.");
    // Now rank the action types and get back the first
    // ranked action type
    actionType = actionRankTypes();

    // The action types should be ranked according to energy, the
    // smallest first.  The smallest is the type at the end of
    // the action list
    for (x = MAX_NUM_ACTIONS - 1; (actionType != ACTION_TYPE_NULL) && (x >= 0); x--) {
        TEST_ASSERT(actionType == gpAction[x]->type);
        y++;
        actionType = actionNextType();
    }
    TEST_ASSERT(y == MAX_NUM_ACTION_TYPES - 1); // -1 to omit ACTION_TYPE_NULL
}

// Test of ranking actions by desirability
void test_rank_desirable() {
    int actionType = ACTION_TYPE_NULL + 1;
    Action *pAction;
    int x = 0;
    int y = 0;

    actionInit();

    // Fill up the action list
    for (pAction = pActionAdd((ActionType) actionType); pAction != NULL; pAction = pActionAdd((ActionType) actionType), x++) {
        gpAction[x] = pAction;
        actionType++;
        if (actionType >= MAX_NUM_ACTION_TYPES) {
            actionType = ACTION_TYPE_NULL + 1;
        }
    }
    TEST_ASSERT(x == MAX_NUM_ACTIONS);

    tr_debug("%d actions added.", x);

    // Set up the desirability for each action type (apart from the NULL one),
    // with the lower action types being least desirable
    for (x = ACTION_TYPE_NULL + 1; x < MAX_NUM_ACTION_TYPES; x++) {
        TEST_ASSERT(actionSetDesirability((ActionType) x, DESIRABILITY_DEFAULT + y));
        y++;
    }

    tr_debug("Ranking actions by desirability, most desirable first.");
    // Now rank the action types and get back the first
    // ranked action type
    actionType = actionRankTypes();

    // The action types should be ranked according to desirability, the
    // most desirable (highest number) first.  The highest number is the
    // type at the end of the action list
    y = 0;
    for (x = MAX_NUM_ACTIONS - 1; (actionType != ACTION_TYPE_NULL) && (x >= 0); x--) {
        TEST_ASSERT(actionType == gpAction[x]->type);
        y++;
        actionType = actionNextType();
    }

    TEST_ASSERT(y == MAX_NUM_ACTION_TYPES - 1); // -1 to omit ACTION_TYPE_NULL

    // Reset desirability to all defaults for the next test
    for (x = ACTION_TYPE_NULL + 1; x < MAX_NUM_ACTION_TYPES; x++) {
        TEST_ASSERT(actionSetDesirability((ActionType) x, DESIRABILITY_DEFAULT));
    }
}

// Test of ranking actions by variability
void test_rank_variable() {
    int actionType = ACTION_TYPE_NULL + 1;
    Action *pAction;
    int x = 0;
    int y = 0;

    actionInit();

    // Fill up the action list
    for (pAction = pActionAdd((ActionType) actionType); pAction != NULL; pAction = pActionAdd((ActionType) actionType), x++) {
        gpAction[x] = pAction;
        actionType++;
        if (actionType >= MAX_NUM_ACTION_TYPES) {
            actionType = ACTION_TYPE_NULL + 1;
        }
    }
    TEST_ASSERT(x == MAX_NUM_ACTIONS);

    tr_debug("%d actions added.", x);

    // Add some data which should cause the rank of the actions to be reversed,
    // which involves going twice around all the action types from the bottom of the
    // list and making sure the difference between the values of the same action type
    // differs appropriately.
    for (x = MAX_NUM_ACTIONS - 1; x > MAX_NUM_ACTIONS - 1 - (MAX_NUM_ACTION_TYPES - 1); x--) { // -1 to skip NULL action
        addData(gpAction[x], 1);
    }
    y = MAX_NUM_ACTION_TYPES + 10; // Doesn't matter what the number is as long as it's larger
                                   // than MAX_NUM_ACTION_TYPES
    for (x = MAX_NUM_ACTIONS - 1 - (MAX_NUM_ACTION_TYPES - 1); x > MAX_NUM_ACTIONS - 1 - ((MAX_NUM_ACTION_TYPES - 1) * 2); x--) {
        addData(gpAction[x], y);
        y--;
    }

    tr_debug("Ranking actions by variability, most variable first.");
    // Now rank the action types and get back the first
    // ranked action type
    actionType = actionRankTypes();

    // The action types should be ranked according to variability, the
    // one with the largest variability first.  The highest number is
    // the type at the end of the action list
    y = 0;
    for (x = MAX_NUM_ACTIONS - 1; (actionType != ACTION_TYPE_NULL) && (x >= 0); x--) {
        TEST_ASSERT(actionType == gpAction[x]->type);
        y++;
        actionType = actionNextType();
    }

    TEST_ASSERT(y == MAX_NUM_ACTION_TYPES - 1); // -1 to omit ACTION_TYPE_NULL

    // Free up the data values that were added
    freeData();
}

// Test the effect of setting desirability to 0
void test_rank_desirable_0() {
    int actionType = ACTION_TYPE_NULL + 1;
    Action *pAction;
    int x = 0;
    int y = 0;
    bool actionTypePresent[MAX_NUM_ACTION_TYPES];

    actionInit();

    // All actions are expected to begin with
    memset (&actionTypePresent, true, sizeof (actionTypePresent));

    // Fill up the action list
    for (pAction = pActionAdd((ActionType) actionType); pAction != NULL; pAction = pActionAdd((ActionType) actionType), x++) {
        gpAction[x] = pAction;
        actionType++;
        if (actionType >= MAX_NUM_ACTION_TYPES) {
            actionType = ACTION_TYPE_NULL + 1;
        }
    }
    TEST_ASSERT(x == MAX_NUM_ACTIONS);

    tr_debug("%d actions added.", x);

    // Set the desirability for the first, last and every even action type (avoiding the NULL one),
    // to zero
    for (x = ACTION_TYPE_NULL + 1; x < MAX_NUM_ACTION_TYPES; x++) {
        if ((x == ACTION_TYPE_NULL + 1) || ((x & 0x01) == 1) || (x == MAX_NUM_ACTION_TYPES - 1)) {
            actionTypePresent[x] = false;
            TEST_ASSERT(actionSetDesirability((ActionType) x, 0));
        }
    }

    tr_debug("Checking that actions with zero desirability disappear.");
    // Rank the action types
    actionType = actionRankTypes();

    // Check that the expected ones, and only the expected ones, have disappeared
    for (x = ACTION_TYPE_NULL + 1; x < ARRAY_SIZE(actionTypePresent); x++) {
        if (actionTypePresent[x]) {
            TEST_ASSERT(actionType == x);
            actionType = actionNextType();
        }
    }
    TEST_ASSERT(actionType == ACTION_TYPE_NULL);

    // Reset desirability to all defaults for the next test
    for (x = ACTION_TYPE_NULL + 1; x < MAX_NUM_ACTION_TYPES; x++) {
        TEST_ASSERT(actionSetDesirability((ActionType) x, DESIRABILITY_DEFAULT));
    }
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
    Case("Initial acions", test_initial_actions),
    Case("Add actions", test_add),
    Case("Rank by rarity", test_rank_rarity),
    Case("Rank by time", test_rank_time),
    Case("Rank by energy", test_rank_energy),
    Case("Rank by desirability", test_rank_desirable),
    Case("Rank by variability", test_rank_variable),
    Case("Switch off with desirability 0", test_rank_desirable_0)
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
