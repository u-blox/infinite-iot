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
 * MANIFEST CONSTANTS: CELLULAR
 *************************************************************************/

/** How long to wait for a network connection.
 */
#define CELLULAR_CONNECT_TIMEOUT_SECONDS 40

/** The credentials of the SIM in the board.  If PIN checking is enabled
 * for your SIM card you must set this to the required PIN.
 */
#define SIM_PIN "0000"

/** Cellular network APN.
 */
#define APN         NULL

/** Username for the cellular network APN.
 */
#define USERNAME    NULL

/** Password for the cellular network APN.
 */
#define PASSWORD    NULL

/** IP address of an NTP server: note that this must be an IP
 * address rather than a URL since SARA-N2xx does not perform
 * DNS resolution.
 * 195.195.221.100:123 is an address of 2.pool.ntp.org.
 */
#define NTP_SERVER_IP_ADDRESS "195.195.221.100"

/** Port for the above NTP server.
 */
#define NTP_SERVER_PORT 123

/** IP address of the target server for coded messages: note
 * that this must be an IP address rather than a URL since
 * SARA-N2xx does not perform DNS resolution.
 * 185.215.195.132:5060 is the address of ciot.it-sgn.u-blox.com.
 */
#define IOT_SERVER_IP_ADDRESS "185.215.195.132"

/** Port for the above server.
 */
#define IOT_SERVER_PORT 5060

/** The socket timeout.
 */
#define SOCKET_TIMEOUT_MS 10000

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

/**************************************************************************
 * MANIFEST CONSTANTS: BLE
 * Note: most of these taken from
 * https://github.com/u-blox/blueprint-B200-NINA-B1/blob/master/Firmware/src/services/uuids.h
 *************************************************************************/

/** The prefix of wanted BLE devices.
 */
#define BLE_PEER_DEVICE_NAME_PREFIX "NINA-B1"

/** The number of data items to retain per device.
 */
#define BLE_PEER_NUM_DATA_ITEMS 2

/** The duration of BLE activity.
 */
#define BLE_ACTIVE_TIME_MS 30000

/** Custom service UUIDs
 */
#define ACC_SRV_UUID    0xFFA0
#define GYRO_SRV_UUID   0xFFB0
#define TEMP_SRV_UUID   0xFFE0
#define LED_SRV_UUID    0xFFD0

/** Accelerometer service characteristics
 */
#define ACC_SRV_UUID_ENABLER    0xFFA1
#define ACC_SRV_UUID_RANGE_CHAR 0xFFA2
#define ACC_SRV_UUID_X_CHAR     0xFFA3
#define ACC_SRV_UUID_Y_CHAR     0xFFA4
#define ACC_SRV_UUID_Z_CHAR     0xFFA5
#define ACC_SRV_UUID_XYZ_CHAR   0xFFA6

/** Gyro service characteristics
 */
#define GYRO_SRV_UUID_ENABLER    0xFFB1
#define GYRO_SRV_UUID_RANGE_CHAR 0xFFB2
#define GYRO_SRV_UUID_X_CHAR     0xFFB3
#define GYRO_SRV_UUID_Y_CHAR     0xFFB4
#define GYRO_SRV_UUID_Z_CHAR     0xFFB5
#define GYRO_SRV_UUID_XYZ_CHAR   0xFFB6

/** Temperature service characteristics
 */
#define TEMP_SRV_UUID_TEMP_CHAR 0xFFE1

/** LED service characteristics
 */
#define LED_SRC_UUID_RED_CHAR   0xFFD1
#define LED_SRC_UUID_GREEN_CHAR 0xFFD2
#define LED_SRC_UUID_BLUE_CHAR  0xFFD3
#define LED_SRC_UUID_RGB_CHAR   0xFFD4

#endif // _EH_CONFIG_H_

// End Of File
