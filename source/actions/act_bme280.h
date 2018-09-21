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

/** How long to wait for a measurement to complete in milliseconds.
 */
#define BME280_MEASUREMENT_WAIT_MS 100

/** The power consumed, in nanoWatts, while the device is off (0.1 uA
 * in sleep mode from table 1 of the datasheet, rounded up here).
 */
#define BME280_POWER_OFF_NW 180

/** The power consumed, in nanoWatts, while the device is ready
 * to take action, which is the same as the off current.
 */
#define BME280_POWER_IDLE_NW 180

/** The energy consumed, in nanoWatt hours, while the device
 * is performing a reading of any type.  From the datasheet
 * Table 1.2 has humidity as 1.8 uA, table 1.3
 * has pressure as 2.8 uA and table 1.4 has
 * temperature as 1 uA.  Each reading type
 * involves temperature followed by the reading requested
 * (or none if it's temperature) but, unfortunately,
 * the datasheet is silent on how long any of the
 * measurements take.  Worst case would be temperature
 * plus pressure at 1 uA + 2.8 uA, which would be 1.9 nWh
 * if taking a reading took 1 second so basically it's no
 * delta on the idle cost.
 */
#define BME280_ENERGY_READING_NWH 0

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
