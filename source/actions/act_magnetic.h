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

#ifndef _ACT_MAGNETIC_H_
#define _ACT_MAGNETIC_H_

#include <act_common.h>

/**************************************************************************
 * MANIFEST CONSTANTS
 *************************************************************************/

/**************************************************************************
 * TYPES
 *************************************************************************/

/**************************************************************************
 * FUNCTIONS
 *************************************************************************/

/** Get the field strength.
 *
 * @param pTeslaX1000   a pointer to a place to put the field strength
 *                      (in milli-Tesla).
 * @return              zero on success or negative error code on failure.
 */
ActionDriver getFieldStrength(unsigned int *pTeslaX1000);

/** Get whether there has been an interrupt from the magnetometer.
 *
 * @return  true if there was an interrupt, else false.
 */
bool getFieldStrengthInterruptFlag();

/** Clear the magnetometer interrupt flag; must be called before
 * the interrupt will go off again.
 */
void clearFieldStrengthInterruptFlag();

#endif // _ACT_MAGNETIC_H_

// End Of File
