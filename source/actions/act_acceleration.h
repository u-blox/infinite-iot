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

#ifndef _ACT_ACCELERATION_H_
#define _ACT_ACCELERATION_H_

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

/** Get the acceleration in x, y and z directions.
 *
 * @param pXGX100  a place to put the X-axis acceleration, measured in milli-g.
 * @param pYGX100  a place to put the Y-axis acceleration, measured in milli-g.
 * @param pZGX100  a place to put the Z-axis acceleration, measured in milli-g.
 * @return         zero on success or negative error code on failure.
 */
ActionDriver getAcceleration(int *pXGX100, int *pYGX1000, int *pZGX1000);

#endif // _ACT_ACCELERATION_H_

// End Of File
