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
 * Note: must be larger than MAX_NUM_ACTION_TYPES and, for all the unit tests
 * to work, should be larger than MAX_NUM_ACTION_TYPES * 2 (since ranking
 * by variability requires at least two of each action type).
 */
#define MAX_NUM_ACTIONS  50

/** The default desirability of an action, noting that this is a signed value.
 */
#define DESIRABILITY_DEFAULT  0

/** The default variability factor for an action.
 */
#define VARIABILITY_FACTOR_DEFAULT 1

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

/** The variability factor of an action.
 */
typedef unsigned int VariabilityFactor;

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

/** Set the desirability value for an action type.
 *
 * @param type          the action type.
 * @param desirability  the desirability value (larger values are more desirable
 *                      and the value is a signed one, hence default is 0).
 * @return              true on success, otherwise false.
 */
bool setDesirability(ActionType type, Desirability desirability);

/** Set the variabilityFactor for an action type.  This acts as a multiplier
 * on the difference between data values so a higher number will tend to schedule
 * an action type more often than other action types as it will appear more
 * variable.
 *
 * @param type              the action type.
 * @param variabilityFactor the variability factor value (default is 1).
 * @return                  true on success, otherwise false.
 */
bool setVariabilityFactor(ActionType type, VariabilityFactor variabilityFactor);

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
 *
 * @return the next action type.
 */
ActionType nextActionType();

/** Rank the action list to produce a list of
 * ranked action types.  The action list is ranked as follows:
 *
 * - rank by variability, most variable first,
 * - rank by desirability, most desirable first,
 * - rank by energy cost, cheapest first,
 * - rank by age, oldest first.
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
