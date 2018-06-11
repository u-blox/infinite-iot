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

#ifndef _ACT_BME280_H_
#define _ACT_BME280_H_

#include <act_common.h>

/**************************************************************************
 * MANIFEST CONSTANTS
 *************************************************************************/

/** Default I2C address for the device with the SDO pin at VDDIO.
 */
#define BME280_DEFAULT_ADDRESS_SDO_VDDIO (0x77)

/** Default I2C address for the device with the SDO pin at GND.
 */
#define BME280_DEFAULT_ADDRESS_SDO_GND (0x76)

/**************************************************************************
 * FUNCTIONS
 *************************************************************************/

/** Initialise the humidity/temperature/pressure sensor BME280.
 * Calling this when the BME280 is already initialised has no effect.
 *
 * @param i2cAddress the address of the BME280 device
 * @return           zero on success or negative error code on failure.
 */
ActionDriver bme280Init(char i2cAddress);

/** Shutdown the humidity/temperature/pressure sensor BME280.
 * Calling this when the BME280 has not been initialised has no effect.
 */
void bme280Deinit();

#endif // _ACT_BME280_H_

// End Of File
