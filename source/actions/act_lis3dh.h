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

#ifndef _ACT_LIS3DH_H_
#define _ACT_LIS3DH_H_

#include <act_common.h>

/**************************************************************************
 * MANIFEST CONSTANTS
 *************************************************************************/

/** Default I2C address when pin SA0 is grounded.
 */
#define LIS3DH_DEFAULT_ADDRESS_SA0_GND (0x18)

/** Default I2C address when pin SA0 is at VSupply.
 */
#define LIS3DH_DEFAULT_ADDRESS_SA0_VSUPPLY (0x19)

/**************************************************************************
 * TYPES
 *************************************************************************/

/**************************************************************************
 * FUNCTIONS
 *************************************************************************/

/** Initialise the orientation sensor LIS3DH.
 * Calling this when the LIS3DH is already initialised has no effect.
 *
 * @param i2cAddress the address of the LIS3DH device
 * @return           zero on success or negative error code on failure.
 */
ActionDriver lis3dhInit(char i2cAddress);

/** Shutdown the orientation sensor LIS3DH.
 * Calling this when the LIS3DH has not been initialised has no effect.
 */
void lis3dhDeinit();

#endif // _ACT_LIS3DH_H_

// End Of File
