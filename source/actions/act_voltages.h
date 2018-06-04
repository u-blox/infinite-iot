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

/**************************************************************************
 * FUNCTIONS
 *************************************************************************/

/** Check if VBAT_OK indicates that the secondary battery is charged enough to
 * run from.
 *
 * @return true if the secondary battery is charged enough to run from,
 *         else false.
 */
bool powerIsGood();

/** Fake power being good; required during unit testing.
 *
 * @param   if true powerIsGood() is faked to true, else it is not.
 */
void fakePowerIsGood(bool fake);

/** Fake power being bad; required during unit testing.
 *
 * @param   if true powerIsGood() is faked to false, else it is not.
 */
void fakePowerIsBad(bool fake);

#endif // _ACT_VOLTAGES_H_

// End Of File
