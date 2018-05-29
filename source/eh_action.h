/*
 * Copyright (C) u-blox Melbourn Ltd
 * u-blox Melbourn Ltd, Melbourn, UK
 *
 * All rights reserved.
 *
 * This source file is the sole property of u-blox Melbourn Ltd.
 * Reproduction or utilisation of this source in whole or part is
 * forbidden without the written consent of u-blox Melbourn Ltd.
 */

#ifndef _EH_ACTION_H_
#define _EH_ACTION_H_

#include <time.h>

/**************************************************************************
 * MANIFEST CONSTANTS
 *************************************************************************/

/** The maximum number of items in the action list.
 * Note: must be larger than MAX_NUM_ACTION_TYPES.
 */
#define MAX_NUM_ACTIONS  20

/** The default desirability of an action, noting that this is a signed value.
 */
#define DESIRABILITY_DEFAULT  0

/**************************************************************************
 * TYPES
 *************************************************************************/

/** The types of action.  If you update this list, please also update
 * gActionTypeString.
 */
typedef enum {
    ACTION_TYPE_NULL,
    ACTION_TYPE_REPORT,
    ACTION_TYPE_GET_TIME_AND_REPORT,
    ACTION_TYPE_MEASURE_HUMIDITY,
    ACTION_TYPE_MEASURE_ATMOSPHERIC_PRESSURE,
    ACTION_TYPE_MEASURE_TEMPERATURE,
    ACTION_TYPE_MEASURE_LIGHT,
    ACTION_TYPE_MEASURE_ORIENTATION,
    ACTION_TYPE_MEASURE_POSITION,
    ACTION_TYPE_MEASURE_MAGNETIC,
    ACTION_TYPE_MEASURE_BLE,
    MAX_NUM_ACTION_TYPES
} ActionType;

/** The possible states an action can be in.  If you update this list,
 * please also update gActionStateString.
 */
typedef enum {
    ACTION_STATE_NULL,
    ACTION_STATE_REQUESTED,
    ACTION_STATE_IN_PROGRESS,
    ACTION_STATE_COMPLETED,
    ACTION_STATE_ABORTED,
    MAX_NUM_ACTION_STATES
} ActionState;

/** The desirability of an action.
 */
typedef signed char Desirability;

/** Definition of an action.
 */
typedef struct {
    time_t timeCompletedUTC;
    unsigned int energyCostUWH;
    void *pData;
    ActionType type;
    ActionState state;
} Action;

/**************************************************************************
 * FUNCTIONS
 *************************************************************************/

/** Initialise the actions lists.
 */
void initActions();

/** Add a new action to the list.
 * Note that actions do not appear in the ranked list until
 * rankActions() is called.
 *
 * @param type the action type to add.
 * @return     a pointer to the action on success, NULL
 *             if it has not been possible to add an action.
 */
Action*pAddAction(ActionType type);

/** Remove an action from the list, freeing up
 * any data associated with the action in the process.
 * Note: it is up to the caller to make sure that the
 * action, or the data associated with the action,
 * are not in use at the time.
 *
 * @param pAction pointer to the action to remove.
 */
void removeAction(Action *pAction);

/** Get the next action type to perform.
 * The next action type is reset to the start of the action list
 * when rankActionTypes() is called.  Will be ACTION_TYPE_NULL if
 * there are no actions left to perform.
 */
ActionType nextActionType();

/** Rank the action list to produce a list of
 * ranked action types.  The action list is ranked as follows:
 *
 * - rank by age, oldest first,
 * - rank by energy cost, cheapest first,
 * - rank by desirability, most desirable first,
 * - rank by variability, most variable first,
 * - rank by the sum of age rank, energy cost, desirability and variability,
 *   lowest to highest.
 *
 * @return the next action type, ACTION_TYPE_NULL if there are none.
 */
ActionType rankActionTypes();

/** Print the action list for debug purposes.
 */
void printActions();

/** Print the ranked action types for debug purposes.
 */
void printRankedActionTypes();

#endif // _EH_ACTION_H_

// End Of File
