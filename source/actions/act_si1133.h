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

#ifndef _ACT_SI1133_H_
#define _ACT_SI1133_H_

#include <act_common.h>

/**************************************************************************
 * MANIFEST CONSTANTS
 *************************************************************************/

/** Default I2C address for the device with the AD pin at VDD.
 */
#define SI1133_DEFAULT_ADDRESS_AD_VDD (0x55)

/** Default I2C address for the device with the AD pin at GND.
 */
#define SI1133_DEFAULT_ADDRESS_AD_GND (0x52)

/** How long to wait for the device to take a reading, in ms.
 */
#define SI1133_WAIT_FOR_READING_MS 1000

/** How long to wait for the device to return to sleep in ms.
 */
#define SI1133_WAIT_FOR_SLEEP_MS 1000

/** How long to wait for the device to absorb a command in ms.
 */
#define SI1133_WAIT_FOR_RESPONSE_MS 1000

/** The power consumed, in nanoWatts, while the device is off.
 */
#define SI1133_POWER_OFF_NW 0

/** The power consumed, in nanoWatts, while the device is
 * on and in standby (0.125 uA @ 1.8V from Table 8.2 of the
 * datasheet).
 */
#define SI1133_POWER_IDLE_NW 225

/** The energy consumed, in nWh, while the device
 * is performing a reading (4.25 mA @ 1.8V for 155 uS
 * from Table 8.2 of the datasheet, which is diddly).
 */
#define SI1133_ENERGY_READING_NWH 0

/**************************************************************************
 * TYPES
 *************************************************************************/

/**************************************************************************
 * FUNCTIONS
 *************************************************************************/

/** Initialise the light sensor SI1133.
 * Calling this when the SI1133 is already initialised has no effect.
 *
 * @param i2cAddress the address of the SI1133 device
 * @return           zero on success or negative error code on failure.
 */
ActionDriver si1133Init(char i2cAddress);

/** Shutdown the light sensor SI1133.
 * Calling this when the SI1133 has not been initialised has no effect.
 */
void si1133Deinit();

#endif // _ACT_SI1133_H_

// End Of File
