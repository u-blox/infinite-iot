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

#ifndef _EH_CONFIG_H_
#define _EH_CONFIG_H_

#include <act_bme280.h>
#include <act_si1133.h>
#include <act_si7210.h>
#include <act_lis3dh.h>

/**************************************************************************
 * MANIFEST CONSTANTS: PINS
 *************************************************************************/

/** Output pin where the debug LED is attached.
 */
#define PIN_DEBUG_LED_BAR          NINA_B1_GPIO_17

/** Output pin to enable 1.8V power to the I2C sensors.
 */
#define PIN_ENABLE_1V8             NINA_B1_GPIO_29

/** Output pin to enable power to the cellular modem.
 */
#define PIN_ENABLE_CDC             NINA_B1_GPIO_28

/** Output pin to *signal* switch-on to the cellular modem.
 * Not used with the SARA_N2xx modem.
 */
#define PIN_CP_ON                  NINA_B1_GPIO_21

/** Output pin to reset the cellular modem.
 * Not available with the SARA_N2xx modem.
 */
#define PIN_CP_RESET_BAR           NINA_B1_GPIO_20

/** Output pin to reset everything.
 */
#define PIN_GRESET_BAR             NINA_B1_GPIO_27

/** Output pin to switch on energy source 1.
 */
#define PIN_ENABLE_ENERGY_SOURCE_1 NINA_B1_GPIO_21 // TODO

/** Output pin to switch on energy source 2.
 */
#define PIN_ENABLE_ENERGY_SOURCE_2 NINA_B1_GPIO_21 // TODO

/** Output pin to switch on energy source 3.
 */
#define PIN_ENABLE_ENERGY_SOURCE_3 NINA_B1_GPIO_21 // TODO

/** VBAT_OK input pin from BQ25505 chip.
 */
#define PIN_VBAT_OK                NINA_B1_GPIO_21 // TODO

/** Input pin for hall effect sensor alert.
 */
#define PIN_INT_MAGNETIC           NINA_B1_GPIO_21 // TODO

/** Input pin for orientation sensor interrupt.
 */
#define PIN_INT_ORIENTATION        NINA_B1_GPIO_21 // TODO

/** Analogue input pin for measuring VIN.
 */
#define PIN_ANALOGUE_VIN           NINA_B1_GPIO_21 // TODO

/** Analogue input pin for measuring VSTOR.
 */
#define PIN_ANALOGUE_VSTOR         NINA_B1_GPIO_21 // TODO

/** Analogue input pin for measuring VPRIMARY.
 */
#define PIN_ANALOGUE_VPRIMARY      NINA_B1_GPIO_21 // TODO

/**************************************************************************
 * MANIFEST CONSTANTS: I2C ADDRESSES
 *************************************************************************/

/** I2C address of the BME280 temperature/humidity/pressure sensor.
 */
#define BME280_DEFAULT_ADDRESS BME280_DEFAULT_ADDRESS_SDO_GND

/** I2C address of the SI1133 light sensor.
 */
#define SI1133_DEFAULT_ADDRESS SI1133_DEFAULT_ADDRESS_AD_GND

/** I2C address of the SI7210 hall effect sensor.
 */
#define SI7210_DEFAULT_ADDRESS SI7210_DEFAULT_ADDRESS_02

/** I2C address of the LIS3DH orientation sensor.
 */
#define LIS3DH_DEFAULT_ADDRESS LIS3DH_DEFAULT_ADDRESS_SA0_GND

#endif // _EH_CONFIG_H_

// End Of File
