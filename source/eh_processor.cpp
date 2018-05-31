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
#include <mbed.h> // For Thread
#include <act_voltages.h>
#include <eh_debug.h>
#include <eh_utilities.h> // For ARRAY_SIZE
#include <eh_processor.h>

/**************************************************************************
 * MANIFEST CONSTANTS
 *************************************************************************/

/**************************************************************************
 * LOCAL VARIABLES
 *************************************************************************/

/** Thread list
 */
static Thread *pThreadList[MAX_NUM_SIMULTANEOUS_ACTIONS];

/**************************************************************************
 * STATIC FUNCTIONS
 *************************************************************************/

// The callback that forms an action thread
static void actionThread(Action *pAction)
{

}

/**************************************************************************
 * PUBLIC FUNCTIONS
 *************************************************************************/

// Initialise the processing system

// Handle wake-up of the system, only returning when it is time to sleep once
// more
void processorHandleWakeup()
{
    ActionType actionType;
    Action *pAction;
    osStatus taskStatus;
    bool keepGoing = true;

    // Only proceed if we have enough power to operate
    if (powerIsGood()) {

        // TODO determine wake-up reason

        // Initialise the thread list
        for (unsigned int x = 0; x < ARRAY_SIZE(pThreadList); x++) {
            pThreadList[x] = NULL;
        }

        // Rank the action log
        actionType = actionRankTypes();

        // Kick off actions while there's power
        for (unsigned int x = 0; (actionType != ACTION_TYPE_NULL) && (x < ARRAY_SIZE(pThreadList)) && keepGoing; x++) {
            pAction = pActionAdd(actionType);
            pThreadList[x] = new Thread(osPriorityNormal, ACTION_TASK_STACK_SIZE);
            if (pThreadList[x] != NULL) {
                taskStatus = pThreadList[x]->start(callback(actionThread, pAction));
                if (taskStatus == osOK) {
                    actionType = actionNextType();
                } else {
                    PRINTF("Error starting task thread (%d).", (int) taskStatus);
                    keepGoing = false;
                }
            } else {
                keepGoing = false;
            }
        }

        // Check VBAT_OK while waiting for actions to complete
        for (unsigned int x = 0; x < ARRAY_SIZE(pThreadList); x++) {

        }
    }

    actionPrint();
    actionPrintRankedTypes();
}

// End of file
