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

#include <time.h>
#include <string.h> // for memset() and memcpy()
#include <eh_data.h>
#include <eh_utilities.h> // For LOCK()/UNLOCK()
#include <eh_statistics.h>

/**************************************************************************
 * MANIFEST CONSTANTS
 *************************************************************************/

// A bounds check for the time
#define EARLIEST_TIME 1529687605

/**************************************************************************
 * LOCAL VARIABLES
 *************************************************************************/

/** The statistics structure.
 */
static DataStatistics gStatistics;

/** The last wake-up time.
 */
time_t gLastWakeUpTime;

/** The last sleep time.
 */
time_t gLastSleepTime;

/**************************************************************************
 * STATIC FUNCTIONS
 *************************************************************************/

// Return the number of seconds since midnight for a given Unix time value.
static time_t secondsSinceMidnight(time_t t)
{
    struct tm *pT = gmtime(&t);
    return pT->tm_hour * 3600 + pT->tm_min * 60 + pT->tm_sec;
}

// Zero statistics that are accumulated on a daily basis
static void zeroDailys()
{
    gStatistics.energyPerDayNWH = 0;
    memset(gStatistics.actionsPerDay, 0, sizeof(gStatistics.actionsPerDay));

}

/**************************************************************************
 * PUBLIC FUNCTIONS
 *************************************************************************/

// Initialise statistics.
void statisticsInit()
{
    memset(&gStatistics, 0, sizeof (gStatistics));
    gLastWakeUpTime = 0;
    gLastSleepTime = 0;
}

// Inform statistics of a shift in the system time.
void statisticsTimeUpdate(time_t newTime)
{
    time_t oldNow = time(NULL);

    // Update the stored times to take account of the change
    gLastWakeUpTime += newTime - oldNow;
    gLastSleepTime += newTime - oldNow;
}

// Get the current statistics.
void statisticsGet(DataStatistics *pStatistics)
{
    if (pStatistics != NULL) {
        memcpy (pStatistics, &gStatistics, sizeof (*pStatistics));
    }
}

// Update wake-up stats.
void statisticsWakeUp()
{
    time_t sleepTime = 0;
    time_t sinceMidnight;

    gLastWakeUpTime = time(NULL);

    sinceMidnight = secondsSinceMidnight(gLastWakeUpTime);

    // Work out how long we have slept for
    if (gLastSleepTime > 0) {
        sleepTime = gLastWakeUpTime - gLastSleepTime;

        // Has the day changed while we were asleep?
        if (sinceMidnight < secondsSinceMidnight(gLastSleepTime)) {
            // If so, reduce the sleep time to the number of seconds today
            // and the wake time to zero
            gStatistics.sleepTimePerDaySeconds = sinceMidnight;
            gStatistics.wakeTimePerDaySeconds = 0;
            zeroDailys();
        } else {
            // Otherwise, add the sleepy time on to the current count
            gStatistics.sleepTimePerDaySeconds += sleepTime;
        }
    }
}

// Update sleep stats.
void statisticsSleep()
{
    time_t wakeTime;
    time_t sinceMidnight;

    gLastSleepTime = time(NULL);
    sinceMidnight = secondsSinceMidnight(gLastSleepTime);

    // Work out how long we have been awake for
    wakeTime = gLastSleepTime - gLastWakeUpTime;

    // Has the day changed while we were awake?
    if (sinceMidnight < secondsSinceMidnight(gLastWakeUpTime)) {
        // If so, reduce the wake time to the number of seconds
        // today and the sleep time to zero
        gStatistics.wakeTimePerDaySeconds = sinceMidnight;
        gStatistics.sleepTimePerDaySeconds = 0;
        zeroDailys();
    } else {
        // Otherwise, add the wake time on to the current count
        gStatistics.wakeTimePerDaySeconds += wakeTime;
    }
}

// Update action stats.
void statisticsAddAction(ActionType action)
{
   if (action < MAX_NUM_ACTION_TYPES) {
       gStatistics.actionsPerDay[action]++;
   }
}

// Update energy stats.
void statisticsAddEnergy(uint64_t energyNWH)
{
    gStatistics.energyPerDayNWH += energyNWH;
}

// Increment the number of data connection attempts.
void statisticsIncConnectionAttempts()
{
    gStatistics.cellularConnectionAttemptsSinceReset++;
}

// Increment the number of data connection successes.
void statisticsIncConnectionSuccess()
{
    gStatistics.cellularConnectionSuccessSinceReset++;
}

// Update transmitted bytes.
void statisticsAddTransmitted(unsigned int bytes)
{
    gStatistics.cellularBytesTransmittedSinceReset += bytes;
}

// Update received bytes.
void statisticsAddReceived(unsigned int bytes)
{
    gStatistics.cellularBytesReceivedSinceReset += bytes;
}

// Increment the number of position measurement attempts.
void statisticsIncPositionAttempts()
{
    gStatistics.positionAttemptsSinceReset++;
}

// Increment the number of position measurement successes.
void statisticsIncPositionSuccess()
{
    gStatistics.positionSuccessSinceReset++;
}

// Update the number of space vehicles visible on the last
// position measurement attempt.
void statisticsLastSVs(unsigned char svs)
{
    gStatistics.positionLastNumSvVisible = svs;
}

// End of file
