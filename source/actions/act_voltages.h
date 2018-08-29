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

/* The value at which VBAT_OK means what it says.  The range of the VBAT_OK
 * reading is 0 to 65535
 */
#define VBAT_OK_THRESHOLD 40000

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
