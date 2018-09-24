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

#ifndef _ACT_VOLTAGES_H_
#define _ACT_VOLTAGES_H_

/**************************************************************************
 * MANIFEST CONSTANTS
 *************************************************************************/

/* The value at which VBAT_OK is good enough to run everything.
 */
#define VBAT_OK_GOOD_THRESHOLD_MV 3600

/* The value at which VBAT_OK is good enough to try to run something.
 */
#define VBAT_OK_BEARABLE_THRESHOLD_MV 3300

/* The value at which VBAT_OK is no longer good enough to do anything.
 */
#define VBAT_OK_BAD_THRESHOLD_MV 3000

/**************************************************************************
 * FUNCTIONS
 *************************************************************************/

/** Get the value of VBAT_OK.
 *
 * @return VBAT_OK in milliVolts.
 */
int getVBatOkMV();

/** Get the value of VIN.
 *
 * @return VIN in milliVolts.
 */
int getVInMV();

/** Get the value of VPRIMARY.
 *
 * @return VPRIMARY in milliVolts.
 */
int getVPrimaryMV();

/* NOTE: there are a few functions here to check
 * the state of the energy supply, they should be
 * used as follows:
 *
 * - at startup, require voltageIsGood() to return
 *   true.  This ensures that there's plenty of
 *   juice in the system to successfully get through
 *   POST and to get through the first few wake-ups
 *   where the system is learning the energy cost of
 *   each type of operation.
 * - at each wake-up, require voltageIsBearable() to
 *   return true.  This ensure that there's a fighting
 *   chance of doing something, though possibly
 *   not the more expensive things (location acquisition
 *   and running the modem).
 * - at the start of the processing wake-up, count up the
 *   energy cost of the actions to be performed and throw
 *   out any actions that cause the energy requirement
 *   to exceed getEnergyOptimisticNWH().  Also check the
 *   length of the data item queue and throw out any
 *   actions for which there is already a lot of data
 *   queued.  This _should_ mean that even the expensive
 *   actions get performed at some point.
 * - during the processing wake-up, call voltageIsNotBad()
 *   on a regular basis and, if it is ever false, cancel
 *   all outstanding actions and return to sleep.
 */

/** Get an optimistic estimate of energy available.
 *
 * @return the energy available in NWH.
 */
unsigned int getEnergyOptimisticNWH();

/** Check if VBAT_OK indicates that the secondary battery is charged enough to
 * run everything from.
 *
 * @return true if the secondary battery is charged enough to run everything,
 *         else false.
 */
bool voltageIsGood();

/** Check if VBAT_OK indicates that the secondary battery is charged enough to
 * run something from.
 *
 * @return true if the secondary battery has enough charge to run something,
 *         else false.
 */
bool voltageIsBearable();

/** Check if VBAT_OK indicates that the secondary battery is STILL charged
 * enough to run something from.
 *
 * @return true if the secondary battery still has enough charge to run something,
 *         else false.
 */
bool voltageIsNotBad();

/** Fake power being good; required during unit testing.
 *
 * @param   if true powerIsGood() is faked to true, else it is not.
 */
void voltageFakeIsGood(bool fake);

/** Fake power being bad; required during unit testing.
 *
 * @param   if true powerIsGood() is faked to false, else it is not.
 */
void voltageFakeIsBad(bool fake);

#endif // _ACT_VOLTAGES_H_

// End Of File
