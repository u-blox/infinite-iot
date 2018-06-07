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

#ifndef _ACT_LIGHT_H_
#define _ACT_LIGHT_H_

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

/** Get the light level and UV index.
 *
 * @param pLux          a pointer to a place to put the light reading (in lux),
 *                      NULL if this is not required.
 * @param pUvIndexX1000 a pointer to a place to put the UV index reading
 *                      (in 1000ths of a unit), NULL if this is not required.
 * @return              zero on success or negative error code on failure.
 */
ActionDriver getLight(int *pLux, int *pUvIndexX1000);

#endif // _ACT_LIGHT_H_

// End Of File
