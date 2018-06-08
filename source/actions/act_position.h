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

#ifndef _ACT_POSITION_H_
#define _ACT_POSITION_H_

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

/** Get the position from GNSS.
 *
 * @param pLatitudeX1000   a place to put latitude (in 1000ths of a degree).
 * @param pLongitudeX1000  a place to put longitude (in 1000ths of a degree).
 * @param pRadiusMetres    a place to put the radius of position (in metres).
 * @param pAltitudeMetres  a place to put the altitude (in metres).
 * @param pSpeedMPS        a place to put the speed (in metres per second).
 * @return                 zero on success or negative error code on failure.
 */
ActionDriver getPosition(int *pLatitudeX1000, int *pLongitudeX1000,
                         int *pRadiusMetres, int *pAltitudeMetres,
                         unsigned char *pSpeedMPS);

#endif // _ACT_ORIENTATION_H_

// End Of File
