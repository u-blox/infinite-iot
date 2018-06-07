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

#ifndef _ACT_SI7210_H_
#define _ACT_SI7210_H_

#include <act_common.h>

/**************************************************************************
 * MANIFEST CONSTANTS
 *************************************************************************/

/** Default I2C address for the devices Si7210-B-00-IV(R)/Si7210-B-01-IV(R).
 */
#define SI7210_DEFAULT_ADDRESS_00_01 (0x30)

/** Default I2C address for the device Si7210-B-02-IV(R).
 */
#define SI7210_DEFAULT_ADDRESS_02 (0x31)

/** Default I2C address for the device Si7210-B-03-IV(R).
 */
#define SI7210_DEFAULT_ADDRESS_03 (0x32)

/** Default I2C address for the devices Si7210-B-04-IV(R)/Si7210-B-05-IV(R).
 */
#define SI7210_DEFAULT_ADDRESS_04_05 (0x32)

/**************************************************************************
 * TYPES
 *************************************************************************/

/**************************************************************************
 * FUNCTIONS
 *************************************************************************/

/** Initialise the hall effect sensor SI7210.
 *
 * @param i2cAddress the address of the SI7210 device
 * @return           zero on success or negative error code on failure.
 */
ActionDriver si7210Init(char i2cAddress);

/** Shutdown the hall effect sensor SI7210.
 */
void si7210Deinit();

#endif // _ACT_SI7210_H_

// End Of File
