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
#include <act_voltages.h>
#include <eh_processor.h>

/**************************************************************************
 * MANIFEST CONSTANTS
 *************************************************************************/

/**************************************************************************
 * LOCAL VARIABLES
 *************************************************************************/

/**************************************************************************
 * STATIC FUNCTIONS
 *************************************************************************/

/**************************************************************************
 * PUBLIC FUNCTIONS
 *************************************************************************/

// Handle wake-up of the system, returning when it is time to sleep once more
void handleWakeup()
{
    ActionType actionType;

    // Only proceed if we have enough power to operate
    if (powerIsGood()) {

        // TODO determine wake-up reason

        // Rank the action log
        actionType = rankActionTypes();

        // TODO kick off actions

        // TODO check VBAT_OK while waiting for actions to complete
    }

    printActions();
    printRankedActionTypes();
}

// End of file
