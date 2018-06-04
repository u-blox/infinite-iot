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

#ifndef _EH_PROCESSOR_H_
#define _EH_PROCESSOR_H_

#include <eh_action.h>

/**************************************************************************
 * MANIFEST CONSTANTS
 *************************************************************************/

/** The stack size to use for each action thread.
 */
#define ACTION_THREAD_STACK_SIZE 500

/** The maximum number of actions to perform at one time.  Each action is
 * run in a separate thread so the limitation is in RAM for the stack of each
 * running task.
 */
#define MAX_NUM_SIMULTANEOUS_ACTIONS 5

/**************************************************************************
 * FUNCTIONS
 *************************************************************************/

/** Initialise the processing system.
 */
void processorInit();

/** Handle wakeup of the system and perform all necessary actions, returning
 * when it is time to go back to sleep again.
 */
void processorHandleWakeup();

/** Set the thread diagnostics callback, required during unit testing.  The
 * callback is called once when an action thread is started and once when
 * it exits.
 *
 * @param threadDiagnosticsCallback  the callback, which must return void and
 *                                   take an Action pointer as a parameter.
 */
void processorSetThreadDiagnosticsCallback(Callback<void(Action *)> threadDiagnosticsCallback);

#endif // _EH_PROCESSOR_H_

// End Of File
