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

#ifndef _ACT_ORIENTATION_H_
#define _ACT_ORIENTATION_H_

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

/** Get the orientation in x, y, z coordinates.
 *
 * @param pX  a place to put the X coordinate.
 * @param pY  a place to put the Y coordinate.
 * @param pZ  a place to put the Z coordinate.
 * @return    zero on success or negative error code on failure.
 */
ActionDriver getOrientation(int *pX, int *pY, int *pZ);

#endif // _ACT_ORIENTATION_H_

// End Of File
