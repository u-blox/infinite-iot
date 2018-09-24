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

#include <mbed_events.h>
#include <eh_action.h>

/**************************************************************************
 * MANIFEST CONSTANTS
 *************************************************************************/

/** The default stack size to use for each action thread.
 */
#define ACTION_THREAD_STACK_SIZE_DEFAULT 2048

/** The maximum number of actions to perform at one time.  Each action is
 * run in a separate thread so the key limitation is in RAM for the stack of each
 * running task.
 */
#define MAX_NUM_SIMULTANEOUS_ACTIONS 3

/** The maximum number of any one data type that we want
 * in the data queue.
 */
#define PROCESSOR_MAX_NUM_DATA_TYPE 20

/** The power consumed by the processor when in RTC sleep, measured
 * at 9 uA @ 1.8 V.
 */
#define PROCESSOR_POWER_IDLE_NW 16200

/** The power consumed by the processor when active, measured
 * at about 4 mA @ 1.8 V.
 */
#define PROCESSOR_POWER_ACTIVE_NW 7200000

/**************************************************************************
 * FUNCTIONS
 *************************************************************************/

/** Initialise the processing system.
 * Note: this will also suspend logging with the expectation that
 * processorHandleWakeup() is the next thing to be called (which will resume
 * logging).
 */
void processorInit();

/** Handle wakeup of the system and perform all necessary actions, including
 * waking up logging, and return when it is time to go back to sleep again.
 *
 * @param pEventQueue a pointer to an event queue (needed by some actions).
 */
void processorHandleWakeup(EventQueue *pEventQueue);

/** Set the thread diagnostics callback, required during unit testing.  The
 * callback is called in the doAction() loop.
 *
 * @param threadDiagnosticsCallback  the callback, which must return a bool,
 *                                   indicating whether the action should
 *                                   continue (true) or not (false) and
 *                                   take an Action pointer as a parameter.
 */
void processorSetThreadDiagnosticsCallback(Callback<bool(Action *)> threadDiagnosticsCallback);

#endif // _EH_PROCESSOR_H_

// End Of File
