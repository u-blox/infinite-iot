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
#include <eh_utilities.h>
#include <eh_debug.h>
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
static Desirability gDesirability[MAX_NUM_ACTION_TYPES] = {DESIRABILITY_DEFAULT};

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
void clearActionList(bool freePData)
{
    for (unsigned int x = 0; x < ARRAY_SIZE(gActionList); x++) {
        gActionList[x].state = ACTION_STATE_NULL;
        if (freePData && (gActionList[x].pData != NULL)) {
            free(gActionList[x].pData);
        }
        gActionList[x].pData = NULL;
    }
}

// Empty the ranked action lists.
// Note: doesn't lock the list.
void clearRankedLists()
{
    for (unsigned int x = 0; x < ARRAY_SIZE(gpRankedList); x++) {
        gpRankedList[x] = NULL;
    }
    for (unsigned int x = 0; x < ARRAY_SIZE(gRankedTypes); x++) {
        gRankedTypes[x] = ACTION_TYPE_NULL;
    }
    gpNextActionType = NULL;
}

// Print an action.
void printAction(Action *pAction)
{
    PRINTF("- %s, %s @%d %d uWh, %s.\n", gActionTypeString[pAction->type],
           gActionStateString[pAction->state], (int) pAction->timeCompletedUTC,
           pAction->energyCostUWH, pAction->pData != NULL ? "has data" : "has no data");
}

// Overwrite an action with new contents.
void writeAction(Action *pAction, ActionType type)
{
    pAction->type = type;
    pAction->state = ACTION_STATE_REQUESTED;
    pAction->timeCompletedUTC = 0;
    pAction->energyCostUWH = 0;
    pAction->pData = NULL;
}

/**************************************************************************
 * PUBLIC FUNCTIONS
 *************************************************************************/

// Initialise the action lists.
void initActions()
{
    LOCK(gMtx);

    // Clear the lists (but don't free pData since it
    // shouldn't have been allocated at this point)
    clearActionList(false);
    clearRankedLists();

    UNLOCK(gMtx);
}

// Remove an action from the list.
void removeAction(Action *pAction)
{
    LOCK(gMtx);

    if (pAction != NULL) {
        CHECK_ACTION_PP(&pAction);
        pAction->state = ACTION_STATE_NULL;
        if (pAction->pData != NULL) {
            free (pAction->pData);
        }
    }

    UNLOCK(gMtx);
}

// Add a new action to the list.
Action *pAddAction(ActionType type)
{
    Action *pAction;

    LOCK(gMtx);

    pAction = NULL;
    // See if there are any NULL or ABORTED entries
    // that can be re-used
    for (unsigned int x = 0; x < ARRAY_SIZE(gActionList); x++) {
        if ((gActionList[x].state == ACTION_STATE_NULL) ||
            (gActionList[x].state == ACTION_STATE_ABORTED)) {
            pAction = &(gActionList[x]);
        }
    }
    // If not, try to re-use a COMPLETED entry
    if (pAction == NULL) {
        for (unsigned int x = 0; x < ARRAY_SIZE(gActionList); x++) {
            if (gActionList[x].state == ACTION_STATE_COMPLETED) {
                pAction = &(gActionList[x]);
            }
        }
    }

    if (pAction != NULL) {
        writeAction(pAction, type);
    }

    UNLOCK(gMtx);

    return pAction;
}

// Get the next action type to perform and advance the action type pointer.
ActionType nextActionType()
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
ActionType rankActionTypes()
{
    Action **ppRanked;
    Action *pRankedTmp;
    unsigned int y;
    bool found;

    LOCK(gMtx);

    // Clear the list
    clearRankedLists();

    ppRanked = &(gpRankedList[0]);
    // Populate the list with the actions have have been used
    for (unsigned int x = 0; x < ARRAY_SIZE(gActionList); x++) {
        if ((gActionList[x].state != ACTION_STATE_NULL) &&
            (gActionList[x].state != ACTION_STATE_ABORTED)) {
            MBED_ASSERT(gActionList[x].type != ACTION_TYPE_NULL);
            *ppRanked = &(gActionList[x]);
            ppRanked++;
        }
    }

    // First, rank by age, oldest first
    ppRanked = &(gpRankedList[0]);
    while (ppRanked < (Action **) (gpRankedList  + ARRAY_SIZE(gpRankedList)) - 1) {
        CHECK_ACTION_PP(ppRanked);
        CHECK_ACTION_PP(ppRanked + 1);
        // If this time is more recent (a higher number) than the next, swap them and restart the sort
        if ((*ppRanked)->timeCompletedUTC > (*(ppRanked + 1))->timeCompletedUTC ) {
            pRankedTmp = *ppRanked;
            *ppRanked = *(ppRanked + 1);
            *(ppRanked + 1) = pRankedTmp;
            ppRanked = &(gpRankedList[0]);
        } else {
            ppRanked++;
        }
    }


    // TODO: rank by energy cost, cheapest first
    // TODO: rank by desirability, most desirable first
    // TODO: rank by variability, most variable first
    // TODO: rank by the sum of age rank, energy cost, desirability and variability, lowest to highest

    // Use the ranked list to assemble the sorted list of action types
    y = 0;
    for (unsigned int x = 0; (x < ARRAY_SIZE(gpRankedList)) && (gpRankedList[x] != NULL); x++) {
        // Check that the type is not already in the list
        found = false;
        for (unsigned int z = 0; (z < ARRAY_SIZE(gRankedTypes)) && !found; z++) {
            found = (gRankedTypes[z] == gpRankedList[x]->type);
        }
        if (!found) {
            MBED_ASSERT(y < ARRAY_SIZE(gRankedTypes));
            gRankedTypes[y] = gpRankedList[x]->type;
        }
        y++;
    }

   // Set the next action type pointer to the start of the ranked types
    gpNextActionType = &(gRankedTypes[0]);

    UNLOCK(gMtx);

    return nextActionType();
}

// Print the action list for debug purposes.
void printActions()
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
void printRankedActionTypes()
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
