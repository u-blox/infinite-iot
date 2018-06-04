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

/** The default desirability of an action.
 */
#define DESIRABILITY_DEFAULT  1

/** The default variability damper for an action.
 */
#define VARIABILITY_DAMPER_DEFAULT 1

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
typedef unsigned char Desirability;

/** The variability damper for an action.
 */
typedef unsigned char VariabilityDamper;

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
void actionInit();

/** Set the desirability value for an action type.
 *
 * @param type          the action type.
 * @param desirability  the desirability value (larger values are more desirable
 *                      and 0 is effectively "off").
 * @return              true on success, otherwise false.
 */
bool actionSetDesirability(ActionType type, Desirability desirability);

/** Set the variability damper for an action type.  This acts as a divisor
 * on the difference between data values.  It defaults to 1; increase this
 * number to de-emphasise actions that have data that is all over the place
 * and drowns out more useful actions.
 *
 * @param type              the action type.
 * @param variabilityDamper the variability damper value (default is 1).
 * @return                  true on success, otherwise false.
 */
bool actionSetVariabilityDamper(ActionType type, VariabilityDamper variabilityDamper);

/** Add a new action to the list.  The action will be added with REQUESTED
 * state at the highest possible entry in the list.
 * Note that actions do not appear in the ranked list until
 * actionRankTypes() is called.
 *
 * @param type the action type to add.
 * @return     a pointer to the action on success, NULL
 *             if it has not been possible to add an action.
 */
Action*pActionAdd(ActionType type);

/** Mark an action as completed.
 * Note: this has no effect on any data that might
 * be associated with the action.
 *
 * @param pAction pointer to the action to remove.
 */
void actionCompleted(Action *pAction);

/** Remove an action from the list.
 * Note: this has no effect on any data that might
 * be associated with the action.
 *
 * @param pAction pointer to the action to remove.
 */
void actionRemove(Action *pAction);

/** Get the next action type to perform.
 * The next action type is reset to the start of the action list
 * when actionRankTypes() is called.  Will be ACTION_TYPE_NULL if
 * there are no actions left to perform.
 *
 * @return the next action type.
 */
ActionType actionNextType();

/** Rank the action list to produce a list of
 * ranked action types.  The action list is ranked as follows:
 *
 * - rank by number of occurrences, least first.
 * - rank by energy cost, cheapest first,
 * - rank by desirability, most desirable first,
 * - rank by variability, most variable first,
 * - rank by time, oldest first.
 *
 * @return the next action type, ACTION_TYPE_NULL if there are none.
 */
ActionType actionRankTypes();

/** Lock the action list.  This may be required by the data
 * module when it is clearing out data. It should not be used
 * by anyone else.  Must be followed by a call to actionUnlockList()
 * or no-one is going to get anywhere.
 */
void actionLockList();

/** Unlock the action list.  This may be required by the data
 * module when it is clearing out data. It should not be used
 * by anyone else.
 */
void actionUnlockList();

/** Print an action for debug purposes.
 */
void actionPrint(Action *pAction);

/** Print the action list for debug purposes.
 */
void actionPrintList();

/** Print the ranked action types for debug purposes.
 */
void actionPrintRankedTypes();

#endif // _EH_ACTION_H_

// End Of File
