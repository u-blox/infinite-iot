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

#ifndef _ACT_ZOEM8_H_
#define _ACT_ZOEM8_H_

#include <act_common.h>

/**************************************************************************
 * MANIFEST CONSTANTS
 *************************************************************************/

/** I2C address of the Zoe M8 GNSS chip.
 */
#define ZOEM8_DEFAULT_ADDRESS 0x21

/**************************************************************************
 * TYPES
 *************************************************************************/

/**************************************************************************
 * FUNCTIONS
 *************************************************************************/

/** Initialise the Zoe M8 GNSS chip.
 *
 * @param i2cAddress the address of the Zoe M8 device
 * @return           zero on success or negative error code on failure.
 */
ActionDriver zoem8Init(char i2cAddress);

/** Shutdown the Zoe M8 GNSS chip.
 */
void zoem8Deinit();

#endif // _ACT_ZOEM8_H_

// End Of File
