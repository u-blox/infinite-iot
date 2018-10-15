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
#define ZOEM8_DEFAULT_ADDRESS 0x42

/** The amount of time to wait for a response from ZOE.
 * Note: when the GNSS chip is busy doing other things it
 * can sometimes take a while to respond.
 */
#define ZOEM8_GET_WAIT_TIME_MS 5000

/** The power consumed, in nanoWatts, while the device is off,
 * which is zero as we switch the supply off.
 */
#define ZOEM8_POWER_OFF_NW 0

/** The power consumed, in nanoWatts, while the device is simply
 * tracking a fix; TODO guess at 5 mA @ 1.8V.
 */
#define ZOEM8_POWER_IDLE_NW 9000000UL

/** The power consumed, in nanoWatts, while the device
 * is obtaining a first fix; from measurements this is about 25 mA
 * @ 1.8V.
 */
#define ZOEM8_POWER_ACTIVE_NW 45000000UL

/**************************************************************************
 * TYPES
 *************************************************************************/

/**************************************************************************
 * FUNCTIONS
 *************************************************************************/

/** Initialise the Zoe M8 GNSS chip.
 * Calling this when the Zoe M8 is already initialised has no effect.
 *
 * @param i2cAddress the address of the Zoe M8 device
 * @return           zero on success or negative error code on failure.
 */
ActionDriver zoem8Init(char i2cAddress);

/** Shutdown the Zoe M8 GNSS chip.
 * Calling this when the Zoe M8 has not been initialised has no effect.
 */
void zoem8Deinit();

#endif // _ACT_ZOEM8_H_

// End Of File
