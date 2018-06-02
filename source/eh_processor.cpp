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
static Thread *pActionThreadList[MAX_NUM_SIMULTANEOUS_ACTIONS];

/**************************************************************************
 * STATIC FUNCTIONS
 *************************************************************************/

// The callback that forms an action thread
static void doAction(Action *pAction)
{
    LOG(EVENT_ACTION_THREAD_STARTED, pAction->type);

    while (Thread::signal_wait(TERMINATE_THREAD_SIGNAL, 0).status == osOK) {
        // Do a thing and check the above condition frequently
    }

    LOG(EVENT_ACTION_THREAD_TERMINATED, pAction->type);
}

// Tidy up any threads that have terminated, returning
// the number still running.
static int checkThreadsRunning()
{
    int numThreadsRunning = 0;

    for (unsigned int x = 0; x < ARRAY_SIZE(pActionThreadList); x++) {
        if (pActionThreadList[x] != NULL) {
            if (pActionThreadList[x]->get_state() ==  rtos::Thread::Deleted) {
                delete pActionThreadList[x];
                pActionThreadList[x] = NULL;
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
    int x;

    // Set the terminate signal on all threads
    for (unsigned int x = 0; x < ARRAY_SIZE(pActionThreadList); x++) {
        if (pActionThreadList[x] != NULL) {
            pActionThreadList[x]->signal_set(TERMINATE_THREAD_SIGNAL);
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
        for (unsigned int x = 0; x < ARRAY_SIZE(pActionThreadList); x++) {
            pActionThreadList[x] = NULL;
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
    if (powerIsGood()) {
        LOG(EVENT_POWER, 1);

        // TODO determine wake-up reason
        LOG(EVENT_WAKE_UP, 0);

        // Rank the action log
        actionType = actionRankTypes();
        LOG(EVENT_ACTION, actionType);
        actionPrintRankedTypes();

        // Kick off actions while there's power and something to start
        while ((actionType != ACTION_TYPE_NULL) && powerIsGood()) {
            // If there's an empty slot, start an action thread
            if (pActionThreadList[taskIndex] == NULL) {
                pAction = pActionAdd(actionType);
                pActionThreadList[taskIndex] = new Thread(osPriorityNormal, ACTION_THREAD_STACK_SIZE);
                if (pActionThreadList[taskIndex] != NULL) {
                    taskStatus = pActionThreadList[taskIndex]->start(callback(doAction, pAction));
                    if (taskStatus != osOK) {
                        LOG(EVENT_ACTION_THREAD_START_FAILURE, taskStatus);
                        delete pActionThreadList[taskIndex];
                        pActionThreadList[taskIndex] = NULL;
                    }
                    actionType = actionNextType();
                    LOG(EVENT_ACTION, actionType);
                } else {
                    LOG(EVENT_ACTION_THREAD_ALLOC_FAILURE, 0);
                }
            }

            taskIndex++;
            if (taskIndex >= ARRAY_SIZE(pActionThreadList)) {
                taskIndex = 0;
                LOG(EVENT_ACTION_THREADS_RUNNING, checkThreadsRunning());
                wait_ms(PROCESSOR_IDLE_MS); // Relax a little once we've set a batch off
            }

            // Check if any threads have ended
            checkThreadsRunning();
        }

        LOG(EVENT_POWER, powerIsGood());

        // If we've got here then either we've kicked off all the required actions or
        // power is no longer good.  While power is good, just do a background check on
        // the progress of the remaining actions.
        while (powerIsGood() && (checkThreadsRunning() > 0)) {
            wait_ms(PROCESSOR_IDLE_MS);
        }

        LOG(EVENT_POWER, powerIsGood());

        // We've now either done everything or power has gone.  If there are threads
        // still running, terminate them.
        terminateAllThreads();

        LOG(EVENT_PROCESSOR_FINISHED, 0);
    }
}

// End of file
