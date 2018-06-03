/* mbed Microcontroller Library
 * Copyright (c) 2006-2018 u-blox Limited
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <mbed.h>
#include <stdlib.h> // for abs()
#include <eh_utilities.h>
#include <eh_debug.h>
#include <eh_data.h>
#include <eh_action.h>

/**************************************************************************
 * MANIFEST CONSTANTS
 *************************************************************************/

/** Macro to check an Action **
 *
 */
#define CHECK_ACTION_PP(ppaction)  \
    MBED_ASSERT ((*(ppaction) < (Action *) (gActionList  + ARRAY_SIZE(gActionList))) || \
                 (*(ppaction) == NULL))

/**************************************************************************
 * LOCAL VARIABLES
 *************************************************************************/

/** Flag so that we know if we've been initialised.
 */
static bool gInitialised = false;

/** The action list.
 */
static Action gActionList[MAX_NUM_ACTIONS];

/** The array used while ranking actions; must be the same size as gActionList.
 */
static Action *gpRankedList[MAX_NUM_ACTIONS];

/** The outcome of ranking the array: a prioritised list of action types.
 */
static ActionType gRankedTypes[MAX_NUM_ACTION_TYPES];

/** Mutex to protect these lists
 */
static Mutex gMtx;

/** A pointer to the next action type to perform.
 */
static ActionType *gpNextActionType = NULL;

/** Desirability table for actions.
 * Note: index into this using ActionType.
 */
static Desirability gDesirability[MAX_NUM_ACTION_TYPES];

/** The variability damper table for actions.
 * Note: index into this using ActionType.
 */
static VariabilityDamper gVariabilityDamper[MAX_NUM_ACTION_TYPES];

/** A pointer to the last data entry for each action type, used when evaluating how variable it is.
 */
static Data *gpLastDataValue[MAX_NUM_ACTION_TYPES] = {NULL};

/** The peak variability table for actions, used as temporary storage when ranking.
 * Note: index into this using ActionType.
 */
static unsigned int gPeakVariability[MAX_NUM_ACTION_TYPES];

#ifdef MBED_CONF_APP_ENABLE_PRINTF
/** The action states as strings for debug purposes.
 */
static const char *gActionStateString[] = {"ACTION_STATE_NULL",
                                           "ACTION_STATE_REQUESTED",
                                           "ACTION_STATE_IN_PROGRESS",
                                           "ACTION_STATE_COMPLETED",
                                           "ACTION_STATE_ABORTED"};

/** The action types as strings for debug purposes.
 */
static const char *gActionTypeString[] = {"ACTION_TYPE_NULL",
                                          "ACTION_TYPE_REPORT",
                                          "ACTION_TYPE_GET_TIME_AND_REPORT",
                                          "ACTION_TYPE_MEASURE_HUMIDITY",
                                          "ACTION_TYPE_MEASURE_ATMOSPHERIC_PRESSURE",
                                          "ACTION_TYPE_MEASURE_TEMPERATURE",
                                          "ACTION_TYPE_MEASURE_LIGHT",
                                          "ACTION_TYPE_MEASURE_ORIENTATION",
                                          "ACTION_TYPE_MEASURE_POSITION",
                                          "ACTION_TYPE_MEASURE_MAGNETIC",
                                          "ACTION_TYPE_MEASURE_BLE"};
#endif

/**************************************************************************
 * STATIC FUNCTIONS
 *************************************************************************/

// Empty the action list.
// Note: doesn't lock the list.
static void clearActionList(bool freeData)
{
    for (unsigned int x = 0; x < ARRAY_SIZE(gActionList); x++) {
        gActionList[x].state = ACTION_STATE_NULL;
        if (freeData && (gActionList[x].pData != NULL)) {
            free(gActionList[x].pData);
        }
        gActionList[x].pData = NULL;
    }
}

// Empty the ranked action lists.
// Note: doesn't lock the list.
static void clearRankedLists()
{
    for (unsigned int x = 0; x < ARRAY_SIZE(gpRankedList); x++) {
        gpRankedList[x] = NULL;
    }
    for (unsigned int x = 0; x < ARRAY_SIZE(gRankedTypes); x++) {
        gRankedTypes[x] = ACTION_TYPE_NULL;
    }
    for (unsigned int x = 0; x < ARRAY_SIZE(gPeakVariability); x++) {
        gPeakVariability[x] = 0;
    }
    for (unsigned int x = 0; x < ARRAY_SIZE(gpLastDataValue); x++) {
        gpLastDataValue[x] = NULL;
    }
    gpNextActionType = NULL;
}

// Print an action.
static void printAction(Action *pAction)
{
    PRINTF("- %s, %s @%d %d uWh, %s.\n", gActionTypeString[pAction->type],
           gActionStateString[pAction->state], (int) pAction->timeCompletedUTC,
           pAction->energyCostUWH, pAction->pData != NULL ? "has data" : "has no data");
}

// Overwrite an action with new contents.
static void writeAction(Action *pAction, ActionType type)
{
    pAction->type = type;
    pAction->state = ACTION_STATE_REQUESTED;
    pAction->timeCompletedUTC = 0;
    pAction->energyCostUWH = 0;
    // Unhook any data items that might have been
    // attached to a completed action.  Don't
    // try to free them though, they have a life
    // of their own
    dataLockList();
    if (pAction->pData != NULL) {
        ((Data *) pAction->pData)->pAction = NULL;
        pAction->pData = NULL;
    }
    dataUnlockList();
}

// Condition function to return true if pNextAction is older (a lower
// number) than pAction.
static bool conditionOlder(Action *pAction, Action *pNextAction)
{
    return pNextAction->timeCompletedUTC < pAction->timeCompletedUTC;
}

// Condition function to return true if pNextAction is more efficient
// (a lower number) than pAction.
static bool conditionLessEnergy(Action *pAction, Action *pNextAction)
{
    return pNextAction->energyCostUWH < pAction->energyCostUWH;
}

// Condition function to return true if pNextAction is more desirable (a
// higher number) than pAction.
static bool conditionMoreDesirable(Action *pAction, Action *pNextAction)
{
    return gDesirability[pNextAction->type] > gDesirability[pAction->type];
}

// Condition function to return true if pNextAction exceeds its variability
// threshold more than pAction.
static bool conditionMoreVariable(Action *pAction, Action *pNextAction)
{
    return gPeakVariability[pNextAction->type] > gPeakVariability[pAction->type];
}

// Rank the gpRankedList using the given condition function.
// NOTE: this does not lock the list.
static void ranker(bool condition(Action *, Action *)) {
    Action **ppRanked;
    Action *pRankedTmp;

    ppRanked = &(gpRankedList[0]);
    while (ppRanked < (Action **) (gpRankedList  + ARRAY_SIZE(gpRankedList)) - 1) {
        CHECK_ACTION_PP(ppRanked);
        CHECK_ACTION_PP(ppRanked + 1);
        // If condition is true, swap them and restart the sort
        if (condition(*ppRanked, *(ppRanked + 1))) {
            pRankedTmp = *ppRanked;
            *ppRanked = *(ppRanked + 1);
            *(ppRanked + 1) = pRankedTmp;
            ppRanked = &(gpRankedList[0]);
        } else {
            ppRanked++;
        }
    }
}

/**************************************************************************
 * PUBLIC FUNCTIONS
 *************************************************************************/

// Initialise the action lists.
void actionInit()
{
    LOCK(gMtx);

    // Clear the lists (but only free data if we've been initialised before)
    clearActionList(gInitialised);
    clearRankedLists();

    for (unsigned int x = 0; x < ARRAY_SIZE(gDesirability); x++) {
        gDesirability[x] = DESIRABILITY_DEFAULT;
    }

    for (unsigned int x = 0; x < ARRAY_SIZE(gVariabilityDamper); x++) {
        gVariabilityDamper[x] = VARIABILITY_DAMPER_DEFAULT;
    }

    gInitialised = true;

    UNLOCK(gMtx);
}

// Set the desirability of an action type.
bool actionSetDesirability(ActionType type, Desirability desirability)
{
    bool success = false;

    if (type < ARRAY_SIZE(gDesirability)) {
        gDesirability[type] = desirability;
        success = true;
    }

    return success;
}

// Set the variability factor of an action type.
bool actionSetVariabilityFactor(ActionType type, VariabilityDamper variabilityDamper)
{
    bool success = false;

    if (type < ARRAY_SIZE(gVariabilityDamper)) {
        gVariabilityDamper[type] = variabilityDamper;
        success = true;
    }

    return success;
}

// Mark an action as completed.
void actionCompleted(Action *pAction)
{
    LOCK(gMtx);

    if (pAction != NULL) {
        CHECK_ACTION_PP(&pAction);
        pAction->state = ACTION_STATE_COMPLETED;
    }

    UNLOCK(gMtx);
}

// Remove an action from the list.
void actionRemove(Action *pAction)
{
    LOCK(gMtx);

    if (pAction != NULL) {
        CHECK_ACTION_PP(&pAction);
        pAction->state = ACTION_STATE_NULL;
    }

    UNLOCK(gMtx);
}

// Add a new action to the list.
Action *pActionAdd(ActionType type)
{
    Action *pAction;

    LOCK(gMtx);

    pAction = NULL;
    // See if there are any NULL or ABORTED entries
    // that can be re-used
    for (unsigned int x = 0; (x < ARRAY_SIZE(gActionList)) && (pAction == NULL); x++) {
        if ((gActionList[x].state == ACTION_STATE_NULL) ||
            (gActionList[x].state == ACTION_STATE_ABORTED)) {
            pAction = &(gActionList[x]);
        }
    }
    // If not, try to re-use a COMPLETED entry
    for (unsigned int x = 0; (x < ARRAY_SIZE(gActionList)) && (pAction == NULL); x++) {
        if (gActionList[x].state == ACTION_STATE_COMPLETED) {
            pAction = &(gActionList[x]);
        }
    }

    if (pAction != NULL) {
        writeAction(pAction, type);
    }

    UNLOCK(gMtx);

    return pAction;
}

// Get the next action type to perform and advance the action type pointer.
ActionType actionNextType()
{
    ActionType actionType;

    LOCK(gMtx);

    actionType = ACTION_TYPE_NULL;
    if (gpNextActionType != NULL) {
        actionType = *gpNextActionType;
        if (gpNextActionType < (ActionType *) (gRankedTypes  + ARRAY_SIZE(gRankedTypes))) {
            gpNextActionType++;
        } else {
            gpNextActionType = NULL;
        }
    }

    UNLOCK(gMtx);

    return actionType;
}

// Create the ranked the action type list.
ActionType actionRankTypes()
{
    Action **ppRanked;
    unsigned int z;
    bool found;

    LOCK(gMtx);

    // Clear the lists
    clearRankedLists();

    ppRanked = &(gpRankedList[0]);
    // Populate the list with the actions that have been used
    // working out the peak variability of each one along the way
    for (unsigned int x = 0; x < ARRAY_SIZE(gActionList); x++) {
        if ((gActionList[x].state != ACTION_STATE_NULL) &&
            (gActionList[x].state != ACTION_STATE_ABORTED)) {
            MBED_ASSERT(gActionList[x].type != ACTION_TYPE_NULL);
            if (gActionList[x].pData != NULL) {
                // If the action has previous data, work out how much it
                // differs from this previous data and divide by the
                // variability damper
                if (gpLastDataValue[gActionList[x].type] != NULL) {
                    z = abs(dataDifference(gpLastDataValue[gActionList[x].type],
                                           (Data *) gActionList[x].pData));
                    z = z / gVariabilityDamper[gActionList[x].type];
                    if (z > gPeakVariability[gActionList[x].type]) {
                        gPeakVariability[gActionList[x].type] = z;
                    }
                }
                gpLastDataValue[gActionList[x].type] = (Data *) gActionList[x].pData;
            }
            *ppRanked = &(gActionList[x]);
            ppRanked++;
        }
    }

    // Rank by variability, most variable first
    ranker(&conditionMoreVariable);
    // Rank by desirability, most desirable first
    ranker(&conditionMoreDesirable);
    // Now rank by energy cost, cheapest first
    ranker(&conditionLessEnergy);
    // First, rank by age, oldest first
    ranker(&conditionOlder);

    // Use the ranked list to assemble the sorted list of action types
    z = 0;
    for (unsigned int x = 0; (x < ARRAY_SIZE(gpRankedList)) && (gpRankedList[x] != NULL); x++) {
        // Check that the type is not already in the list
        found = false;
        for (unsigned int y = 0; (y < ARRAY_SIZE(gRankedTypes)) && !found; y++) {
            found = (gRankedTypes[y] == gpRankedList[x]->type);
        }
        if (!found) {
            MBED_ASSERT(z < ARRAY_SIZE(gRankedTypes));
            gRankedTypes[z] = gpRankedList[x]->type;
            z++;
        }
    }

   // Set the next action type pointer to the start of the ranked types
    gpNextActionType = &(gRankedTypes[0]);

    UNLOCK(gMtx);

    return actionNextType();
}

// Lock the action list.
void actionLockList()
{
    gMtx.lock();
}

// Unlock the action list.
void actionUnlockList()
{
    gMtx.unlock();
}

// Print the action list for debug purposes.
void actionPrint()
{
    int numActions;

    LOCK(gMtx);

    numActions = 0;
    PRINTF("Action list:\n");
    for (unsigned int x = 0; (x < ARRAY_SIZE(gpRankedList)) && (gpRankedList[x] != NULL); x++) {
        if ((gActionList[x].state != ACTION_STATE_NULL) &&
            (gActionList[x].state != ACTION_STATE_ABORTED)) {
            printAction(&(gActionList[x]));
            numActions++;
        }
    }

    PRINTF("  %d action(s) in the list.\n", numActions);

    UNLOCK(gMtx);
}

// Print the ranked action types for debug purposes.
void actionPrintRankedTypes()
{
    int numActionTypes;

    LOCK(gMtx);

    numActionTypes = 0;
    PRINTF("Ranked action types:\n");
    for (unsigned int x = 0; (x < ARRAY_SIZE(gRankedTypes)) && (gRankedTypes[x] != ACTION_TYPE_NULL); x++) {
        numActionTypes++;
        PRINTF("%d: %s.\n", numActionTypes, gActionTypeString[gRankedTypes[x]]);
    }

    PRINTF("  %d action type(s) in the list.\n", numActionTypes);

    UNLOCK(gMtx);
}

// End of file
