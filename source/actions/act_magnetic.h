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

/** Possible (bipolar) measurement range settings.
 */
typedef enum {
    RANGE_20_MICRO_TESLAS,
    RANGE_200_MICRO_TESLAS
} FieldStrengthRange;

/**************************************************************************
 * FUNCTIONS
 *************************************************************************/

/** Get the field strength.
 *
 * @param pTeslaX1000   a pointer to a place to put the field strength
 *                      (in micro-Tesla).
 * @return              zero on success or negative error code on failure.
 */
ActionDriver getFieldStrength(unsigned int *pTeslaX1000);

/** Set the measurement range (default is RANGE_20_MICRO_TESLAS).
 *
 * @param range the field strength range.
 * @return      zero on success or negative error code on failure.
 */
ActionDriver setRange(FieldStrengthRange range);

/** Get the measurement range.
 *
 * @return      the field strength range.
 */
FieldStrengthRange getRange();

#endif // _ACT_MAGNETIC_H_

// End Of File
