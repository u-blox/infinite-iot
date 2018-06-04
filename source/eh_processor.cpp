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
#include <mbed.h> // For Threading
#include <act_voltages.h> // For powerIsGood()
#include <eh_debug.h> // For LOG
#include <eh_utilities.h> // For ARRAY_SIZE
#include <eh_processor.h>

/**************************************************************************
 * MANIFEST CONSTANTS
 *************************************************************************/

/** Set this signal to end a doAction() thread.
 */
#define TERMINATE_THREAD_SIGNAL 0x01

/** The main processing thread idles for this long when
 * waiting for the other threads to run.
 */
#define PROCESSOR_IDLE_MS 1000

/**************************************************************************
 * LOCAL VARIABLES
 *************************************************************************/

/** Flag so that we know if we've been initialised.
 */
static bool gInitialised = false;

/** Thread list.
 */
static Thread *gpActionThreadList[MAX_NUM_SIMULTANEOUS_ACTIONS];

/** Diagnostic hook.
 */
static Callback<void(Action *)> gThreadDiagnosticsCallback = NULL;

/**************************************************************************
 * STATIC FUNCTIONS
 *************************************************************************/

// Check whether this thread has been terminated.
// Note: the signal is automatigically reset after it has been received,
// hence it is necessary to pass in a pointer to the same "keepGoing" flag
// to remember when it goes south.
static bool threadContinue(bool *pKeepGoing)
{
    return (*pKeepGoing = *pKeepGoing && (Thread::signal_wait(TERMINATE_THREAD_SIGNAL, 0).status == osOK));
}

// The callback that forms an action thread
static void doAction(Action *pAction)
{
    bool keepGoing = true;

    LOG(EVENT_ACTION_THREAD_STARTED, pAction->type);

    if (gThreadDiagnosticsCallback) {
        gThreadDiagnosticsCallback(pAction);
    }

    while (threadContinue(&keepGoing)) {
        // Do a thing and check the above condition frequently
    }

    if (gThreadDiagnosticsCallback) {
        gThreadDiagnosticsCallback(pAction);
    }

    LOG(EVENT_ACTION_THREAD_TERMINATED, pAction->type);
}

// Tidy up any threads that have terminated, returning
// the number still running.
static int checkThreadsRunning()
{
    int numThreadsRunning = 0;

    for (unsigned int x = 0; x < ARRAY_SIZE(gpActionThreadList); x++) {
        if (gpActionThreadList[x] != NULL) {
            if (gpActionThreadList[x]->get_state() ==  rtos::Thread::Deleted) {
                delete gpActionThreadList[x];
                gpActionThreadList[x] = NULL;
            } else {
                numThreadsRunning++;
            }
        }
    }

    return numThreadsRunning;
}

// Terminate all running threads.
static void terminateAllThreads()
{
    unsigned int x;

    // Set the terminate signal on all threads
    for (x = 0; x < ARRAY_SIZE(gpActionThreadList); x++) {
        if (gpActionThreadList[x] != NULL) {
            gpActionThreadList[x]->signal_set(TERMINATE_THREAD_SIGNAL);
            LOG(EVENT_ACTION_THREAD_SIGNALLED, 0);
        }
    }

    // Wait for them all to end
    while ((x = checkThreadsRunning()) > 0) {
        wait_ms(PROCESSOR_IDLE_MS);
        LOG(EVENT_ACTION_THREADS_RUNNING, x);
    }

    LOG(EVENT_ALL_THREADS_TERMINATED, 0);
}

/**************************************************************************
 * PUBLIC FUNCTIONS
 *************************************************************************/

// Initialise the processing system
void processorInit()
{
    if (!gInitialised) {
        // Initialise the action thread list
        for (unsigned int x = 0; x < ARRAY_SIZE(gpActionThreadList); x++) {
            gpActionThreadList[x] = NULL;
        }

        // Seed the action list with one of each type of action, marked
        // as completed so as not to take up space.
        for (int x = ACTION_TYPE_NULL + 1; x < MAX_NUM_ACTION_TYPES; x++) {
            actionCompleted(pActionAdd((ActionType) x));
        }
    }

    gInitialised = true;
}

// Handle wake-up of the system, only returning when it is time to sleep once
// more
void processorHandleWakeup()
{
    ActionType actionType;
    Action *pAction;
    osStatus taskStatus;
    unsigned int taskIndex = 0;

    // TODO decide what power source to use next

    // If there is enough power to operate, perform some actions
    if (voltageIsGood()) {
        LOG(EVENT_POWER, 1);

        // TODO determine wake-up reason
        LOG(EVENT_WAKE_UP, 0);

        // Rank the action log
        actionType = actionRankTypes();
        LOG(EVENT_ACTION, actionType);
        actionPrintRankedTypes();

        // Kick off actions while there's power and something to start
        while ((actionType != ACTION_TYPE_NULL) && voltageIsGood()) {
            // If there's an empty slot, start an action thread
            if (gpActionThreadList[taskIndex] == NULL) {
                pAction = pActionAdd(actionType);
                gpActionThreadList[taskIndex] = new Thread(osPriorityNormal, ACTION_THREAD_STACK_SIZE);
                if (gpActionThreadList[taskIndex] != NULL) {
                    taskStatus = gpActionThreadList[taskIndex]->start(callback(doAction, pAction));
                    if (taskStatus != osOK) {
                        LOG(EVENT_ACTION_THREAD_START_FAILURE, taskStatus);
                        delete gpActionThreadList[taskIndex];
                        gpActionThreadList[taskIndex] = NULL;
                    }
                    actionType = actionNextType();
                    LOG(EVENT_ACTION, actionType);
                } else {
                    LOG(EVENT_ACTION_THREAD_ALLOC_FAILURE, 0);
                }
            }

            taskIndex++;
            if (taskIndex >= ARRAY_SIZE(gpActionThreadList)) {
                taskIndex = 0;
                LOG(EVENT_ACTION_THREADS_RUNNING, checkThreadsRunning());
                wait_ms(PROCESSOR_IDLE_MS); // Relax a little once we've set a batch off
            }

            // Check if any threads have ended
            checkThreadsRunning();
        }

        LOG(EVENT_POWER, voltageIsGood());

        // If we've got here then either we've kicked off all the required actions or
        // power is no longer good.  While power is good, just do a background check on
        // the progress of the remaining actions.
        while (voltageIsGood() && (checkThreadsRunning() > 0)) {
            wait_ms(PROCESSOR_IDLE_MS);
        }

        LOG(EVENT_POWER, voltageIsGood());

        // We've now either done everything or power has gone.  If there are threads
        // still running, terminate them.
        terminateAllThreads();

        LOG(EVENT_PROCESSOR_FINISHED, 0);
    }
}

// Set the thread diagnostics callback.
void processorSetThreadDiagnosticsCallback(Callback<void(Action *)> threadDiagnosticsCallback)
{
    gThreadDiagnosticsCallback = threadDiagnosticsCallback;
}

// End of file
