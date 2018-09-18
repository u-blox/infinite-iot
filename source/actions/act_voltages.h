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

/* The value at which VBAT_OK is good enough to begin doing stuff.
 */
#define VBAT_OK_GOOD_THRESHOLD_MV 3600

/* The value at which VBAT_OK is no longer good enough to do stuff.
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

/** Check if VBAT_OK indicates that the secondary battery is charged enough to
 * run from.
 *
 * @return true if the secondary battery is charged enough to run from,
 *         else false.
 */
bool voltageIsGood();

/** Check if VBAT_OK has not gone bad after initially being good.
 *
 * @return true if the secondary battery still has enough charge to run from,
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
