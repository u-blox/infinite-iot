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

#ifndef _EH_STATISTICS_H_
#define _EH_STATISTICS_H_

#include <eh_data.h>

/**************************************************************************
 * MANIFEST CONSTANTS
 *************************************************************************/

/**************************************************************************
 * TYPES
 *************************************************************************/

/**************************************************************************
 * FUNCTIONS
 *************************************************************************/

/** Initialise statistics.
 */
void statisticsInit();

/** Let statistics know that the system time is about to be updated.
 * This must be done BEFORE set_time() is called.
 *
 * @param newTime  the (UTC Unix) time that is about to be applied.
 */
void statisticsTimeUpdate(time_t newTime);

/** Get the current set of statistics.
 *
 * @param pStatistics a place to put the statistics.
 */
void statisticsGet(DataStatistics *pStatistics);

/** Let statistics know that the system has awoken.
 * This will update wakeUpsPerDay and allow wakeTimePerDaySeconds/
 * sleepTimePerDaySeconds calculation.
 */
void statisticsWakeUp();

/** Let statistics know that the system is going to sleep.
 * This will update allow wakeTimePerDaySeconds/sleepTimePerDaySeconds
 * calculation.
 */
void statisticsSleep();

/** Let statistics know that an action has been requested. This
 * will update the actionsPerDay array.
 */
void statisticsAddAction(ActionType action);

/** Let statistics know that energy has been used.
 *
 * @param energyNWH the energy used in uWatt Hours
 */
void statisticsAddEnergy(unsigned int energyNWH);

/** Increment the number of connection attempts.
 */
void statisticsIncConnectionAttempts();

/** Increment the number of connection successes.
 */
void statisticsIncConnectionSuccess();

/** Let statistics know the number of bytes newly transmitted
 * over cellular.
 *
 * @param bytes the number of bytes to be added to the
 *              transmit count.
 */
void statisticsAddTransmitted(unsigned int bytes);

/** Let statistics know the number of bytes newly received
 * over cellular.
 *
 * @param bytes the number of bytes to be added to the
 *              receive count.
 */
void statisticsAddReceived(unsigned int bytes);

/** Increment the number of position (i.e. GNSS)
 * measurement attempts.
 */
void statisticsIncPositionAttempts();

/** Increment the number of position (i.e. GNSS)
 * measurement successes.
 */
void statisticsIncPositionSuccess();

/** Let statistics now the number of SVs (space
 * vehicles) that could be seen on the last
 * position measurement attempt.
 *
 * @param svs the number of space vehicles that
 *            were visible on the last position
 *            measurement attempt.
 */
void statisticsLastSVs(unsigned char svs);

#endif // _EH_STATISTICS_H_

// End Of File
