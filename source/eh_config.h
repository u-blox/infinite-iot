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
#include <eh_utilities.h> // For xstr()

/**************************************************************************
 * MANIFEST CONSTANTS: MISC
 *************************************************************************/

/** How frequently to update time.
 */
#define TIME_UPDATE_INTERVAL_SECONDS (24 * 3600)

/**************************************************************************
 * MANIFEST CONSTANTS: VERSION
 *************************************************************************/

/** The first digit (of four) of the system version.
 */
#define SYSTEM_VERSION_DIGIT_1 0

/** The second digit (of four) of the system version.
 */
#define SYSTEM_VERSION_DIGIT_2 0

/** The third digit (of four) of the system version.
 */
#define SYSTEM_VERSION_DIGIT_3 0

/** The last digit of the system version.
 */
#define SYSTEM_VERSION_DIGIT_4 1

/** The version string for the system, made up of the above
 */
#define SYSTEM_VERSION_STRING (xstr(SYSTEM_VERSION_DIGIT_1) "." \
                               xstr(SYSTEM_VERSION_DIGIT_2) "." \
                               xstr(SYSTEM_VERSION_DIGIT_3) "." \
                               xstr(SYSTEM_VERSION_DIGIT_4))

/** The version of the system packed into an int with the first digit
 * on the left and the last digit on the right (i.e. little-endian/
 * natural-reading order); printf() this with formatter 0x%08x.
 */
#define SYSTEM_VERSION_INT ((((unsigned int) SYSTEM_VERSION_DIGIT_1) << 24) | \
                            (((unsigned int) SYSTEM_VERSION_DIGIT_2) << 16) | \
                            (((unsigned int) SYSTEM_VERSION_DIGIT_3) << 8) | \
                            (((unsigned int) SYSTEM_VERSION_DIGIT_4)))

/** The version number for this application's log_enum_app.h/log_strings_app.h
 * pair of files: increment this version number when you change the meaning of
 * an existing log item.  There is no _requirement_ to increment it when adding new
 * items, though you may do so.
 */
#define APPLICATION_LOG_VERSION 1

/**************************************************************************
 * MANIFEST CONSTANTS: CELLULAR
 *************************************************************************/

/** How long to wait for a network connection.
 */
#ifdef MBED_CONF_APP_CELLULAR_CONNECT_TIMEOUT_SECONDS
# define CELLULAR_CONNECT_TIMEOUT_SECONDS  MBED_CONF_APP_CELLULAR_CONNECT_TIMEOUT_SECONDS
#else
# define CELLULAR_CONNECT_TIMEOUT_SECONDS 40
#endif

/** The credentials of the SIM in the board.  If PIN checking is enabled
 * for your SIM card you must set this to the required PIN.
 */
#ifdef MBED_CONF_APP_SIM_PIN
# define SIM_PIN  MBED_CONF_APP_SIM_PIN
#else
# define SIM_PIN "0000"
#endif

/** Cellular network APN.
 */
#ifdef MBED_CONF_APP_APN
# define APN         MBED_CONF_APP_APN
#else
# define APN         NULL
#endif

/** Username for the cellular network APN.
 */
#ifdef MBED_CONF_APP_USERNAME
# define USERNAME    MBED_CONF_APP_USERNAME
#else
# define USERNAME    NULL
#endif

/** Password for the cellular network APN.
 */
#ifdef MBED_CONF_APP_PASSWORD
# define PASSWORD   MBED_CONF_APP_PASSWORD
#else
# define PASSWORD   NULL
#endif

/** IP address of an NTP server: note that this must be an IP
 * address rather than a URL since SARA-N2xx does not perform
 * DNS resolution.
 * 195.195.221.100:123 is an address of 2.pool.ntp.org.
 */
#ifdef MBED_CONF_APP_NTP_SERVER_IP_ADDRESS
# define NTP_SERVER_IP_ADDRESS MBED_CONF_APP_NTP_SERVER_IP_ADDRESS
#else
# define NTP_SERVER_IP_ADDRESS "195.195.221.100"
#endif

/** Port for the above NTP server.
 */
#ifdef MBED_CONF_APP_NTP_SERVER_PORT
# define NTP_SERVER_PORT MBED_CONF_APP_NTP_SERVER_PORT
#else
# define NTP_SERVER_PORT 123
#endif

/** IP address of the target server for coded messages: note
 * that this must be an IP address rather than a URL since
 * SARA-N2xx does not perform DNS resolution.
 * 185.215.195.132:5060 is the address of ciot.it-sgn.u-blox.com.
 */
#ifdef MBED_CONF_APP_IOT_SERVER_IP_ADDRESS
# define IOT_SERVER_IP_ADDRESS MBED_CONF_APP_IOT_SERVER_IP_ADDRESS
#else
# define IOT_SERVER_IP_ADDRESS "185.215.195.132"
#endif

/** Port for the above server.
 */
#ifdef MBED_CONF_APP_IOT_SERVER_PORT
# define IOT_SERVER_PORT MBED_CONF_APP_IOT_SERVER_PORT
#else
# define IOT_SERVER_PORT 8080
#endif

/** The socket timeout.
 */
#ifdef MBED_CONF_APP_SOCKET_TIMEOUT_MS
# define SOCKET_TIMEOUT_MS MBED_CONF_APP_SOCKET_TIMEOUT_MS
#else
# define SOCKET_TIMEOUT_MS 2000
#endif

/** Whether acks are required for normal data reports or not.
 * Note: we don't usually want this, however there is no way
 * to tell whether the SARA-R4 modem has finished sending
 * a reports or not before powering it down and so receiving
 * an ack is the only way to reliably send reports.  Still,
 * it costs power and it means that if the server happens
 * to be down then all devices will suck power.
 *  TODO: decide what's best.
 */
#ifdef MBED_CONF_APP_ACK_FOR_REPORTS
# define ACK_FOR_REPORTS  MBED_CONF_APP_ACK_FOR_REPORTS
#else
# define ACK_FOR_REPORTS true
#endif

/** The time to wait for an ack from the server.
 */
#ifdef MBED_CONF_APP_ACK_TIMEOUT_MS
# define ACK_TIMEOUT_MS  MBED_CONF_APP_ACK_TIMEOUT_MS
#else
# define ACK_TIMEOUT_MS (SOCKET_TIMEOUT_MS * 5)
#endif

/**************************************************************************
 * MANIFEST CONSTANTS: PINS
 *************************************************************************/

/** Output pin where the debug LED is attached.
 */
#ifdef MBED_CONF_APP_PIN_DEBUG_LED
# define PIN_DEBUG_LED              MBED_CONF_APP_PIN_DEBUG_LED
#else
# define PIN_DEBUG_LED              NINA_B1_GPIO_16
#endif

/** Output pin to enable 1.8V power to the I2C sensors.
 */
#ifdef MBED_CONF_APP_PIN_ENABLE_1V8
# define PIN_ENABLE_1V8             MBED_CONF_APP_PIN_ENABLE_1V8
#else
# define PIN_ENABLE_1V8             NINA_B1_GPIO_28
#endif

/** Output pin to enable power to the cellular modem.
 */
#ifdef MBED_CONF_APP_PIN_ENABLE_CDC
# define PIN_ENABLE_CDC             MBED_CONF_APP_PIN_ENABLE_CDC
#else
# define PIN_ENABLE_CDC             NINA_B1_GPIO_1
#endif

/** Output pin to *signal* switch-on to the cellular modem.
 * Not used with the SARA_N2xx modem.
 */
#ifdef MBED_CONF_APP_PIN_CP_ON
# define PIN_CP_ON                  MBED_CONF_APP_PIN_CP_ON
#else
# define PIN_CP_ON                  NINA_B1_GPIO_3
#endif

/** Output pin to reset everything.
 */
#ifdef MBED_CONF_APP_PIN_GRESET_BAR
# define PIN_GRESET_BAR             MBED_CONF_APP_PIN_GRESET_BAR
#else
# define PIN_GRESET_BAR             NINA_B1_GPIO_7
#endif

/** Output pin to switch on energy source 1.
 */
#ifdef MBED_CONF_APP_PIN_ENABLE_ENERGY_SOURCE_1
# define PIN_ENABLE_ENERGY_SOURCE_1 MBED_CONF_APP_PIN_ENABLE_ENERGY_SOURCE_1
#else
# define PIN_ENABLE_ENERGY_SOURCE_1 NINA_B1_GPIO_17
#endif

/** Output pin to switch on energy source 2.
 */
#ifdef MBED_CONF_APP_PIN_ENABLE_ENERGY_SOURCE_2
# define PIN_ENABLE_ENERGY_SOURCE_2 MBED_CONF_APP_PIN_ENABLE_ENERGY_SOURCE_2
#else
# define PIN_ENABLE_ENERGY_SOURCE_2 NINA_B1_GPIO_18
#endif

/** Output pin to switch on energy source 3.
 */
#ifdef MBED_CONF_APP_PIN_ENABLE_ENERGY_SOURCE_3
# define PIN_ENABLE_ENERGY_SOURCE_3 MBED_CONF_APP_PIN_ENABLE_ENERGY_SOURCE_3
#else
# define PIN_ENABLE_ENERGY_SOURCE_3 NINA_B1_GPIO_20
#endif

/** Input pin for hall effect sensor alert.
 */
#ifdef MBED_CONF_APP_PIN_INT_MAGNETIC
# define PIN_INT_MAGNETIC           MBED_CONF_APP_PIN_INT_MAGNETIC
#else
# define PIN_INT_MAGNETIC           NINA_B1_GPIO_2
#endif

/** Input pin for orientation sensor interrupt.
 */
#ifdef MBED_CONF_APP_PIN_INT_ACCELERATION
# define PIN_INT_ACCELERATION        MBED_CONF_APP_PIN_INT_ACCELERATION
#else
# define PIN_INT_ACCELERATION        NINA_B1_GPIO_22
#endif

/** Analogue input pin for measuring VIN.
 */
#ifdef MBED_CONF_APP_PIN_ANALOGUE_VIN
# define PIN_ANALOGUE_VIN           MBED_CONF_APP_PIN_ANALOGUE_VIN
#else
# define PIN_ANALOGUE_VIN           NINA_B1_GPIO_25
#endif

/** VBAT_OK from the BQ25505: this is an analogueish digital output
 * in that it is low until VBAT is OK and then if follows VSTOR.
 * Full scale is 1750 mV but this is the output of a voltage divider
 * where 4200 mV across the divider is full scale.  A good value for
 * the VBAT_OK threshold would be 3200 mV.
 */
#ifdef MBED_CONF_APP_PIN_ANALOGUE_VBAT_OK
# define PIN_ANALOGUE_VBAT_OK       MBED_CONF_APP_PIN_ANALOGUE_VBAT_OK
#else
# define PIN_ANALOGUE_VBAT_OK       NINA_B1_GPIO_27
#endif

/** Analogue input pin for measuring VPRIMARY.
 */
#ifdef MBED_CONF_APP_PIN_ANALOGUE_VPRIMARY
# define PIN_ANALOGUE_VPRIMARY      MBED_CONF_APP_PIN_ANALOGUE_VPRIMARY
#else
# define PIN_ANALOGUE_VPRIMARY      NINA_B1_GPIO_24
#endif

/** I2C data pin.
 */
#ifdef MBED_CONF_APP_PIN_I2C_SDA
# define PIN_I2C_SDA                MBED_CONF_APP_PIN_I2C_SDA
#else
# define PIN_I2C_SDA                NINA_B1_GPIO_23
#endif

/** I2C clock pin.
 */
#ifdef MBED_CONF_APP_PIN_I2C_SCL
# define PIN_I2C_SCL                MBED_CONF_APP_PIN_I2C_SCL
#else
# define PIN_I2C_SCL                NINA_B1_GPIO_21
#endif

/**************************************************************************
 * MANIFEST CONSTANTS: I2C ADDRESSES
 *************************************************************************/

/** I2C address of the BME280 temperature/humidity/pressure sensor.
 */
#ifdef MBED_CONF_APP_BME280_DEFAULT_ADDRESS
# define BME280_DEFAULT_ADDRESS MBED_CONF_APP_BME280_DEFAULT_ADDRESS
#else
# define BME280_DEFAULT_ADDRESS BME280_DEFAULT_ADDRESS_SDO_GND
#endif

/** I2C address of the SI1133 light sensor.
 */
#ifdef MBED_CONF_APP_SI1133_DEFAULT_ADDRESS
# define SI1133_DEFAULT_ADDRESS MBED_CONF_APP_SI1133_DEFAULT_ADDRESS
#else
# define SI1133_DEFAULT_ADDRESS SI1133_DEFAULT_ADDRESS_AD_GND
#endif

/** I2C address of the SI7210 hall effect sensor.
 */
#ifdef MBED_CONF_APP_SI7210_DEFAULT_ADDRESS
# define SI7210_DEFAULT_ADDRESS MBED_CONF_APP_SI7210_DEFAULT_ADDRESS
#else
# define SI7210_DEFAULT_ADDRESS SI7210_DEFAULT_ADDRESS_04_05
#endif

/** I2C address of the LIS3DH orientation sensor.
 */
#ifdef MBED_CONF_APP_LIS3DH_DEFAULT_ADDRESS
# define LIS3DH_DEFAULT_ADDRESS MBED_CONF_APP_LIS3DH_DEFAULT_ADDRESS
#else
# define LIS3DH_DEFAULT_ADDRESS LIS3DH_DEFAULT_ADDRESS_SA0_GND
#endif

/**************************************************************************
 * MANIFEST CONSTANTS: LIS3DH ORIENTATION SENSOR
 *************************************************************************/

/** The sensitivity range for LIS3DH (see act_lisdh.h for definition).
 */
#ifdef MBED_CONF_APP_LIS3DH_SENSITIVITY
# define LIS3DH_SENSITIVITY MBED_CONF_APP_LIS3DH_SENSITIVITY
#else
# define LIS3DH_SENSITIVITY 0
#endif

/** The interrupt threshold for the LIS3DH sensor in milli-G.
 */
#ifdef MBED_CONF_APP_LIS3DH_INTERRUPT_THRESHOLD_MG
# define LIS3DH_INTERRUPT_THRESHOLD_MG MBED_CONF_APP_LIS3DH_INTERRUPT_THRESHOLD_MG
#else
# define LIS3DH_INTERRUPT_THRESHOLD_MG 100
#endif

/**************************************************************************
 * MANIFEST CONSTANTS: SI7210 HALL EFFECT SENSOR
 *************************************************************************/

/** The range for the SI7210 (see act_si7210.h for definition).
 */
#ifdef MBED_CONF_APP_SI7210_RANGE
# define SI7210_RANGE MBED_CONF_APP_SI7210_RANGE
#else
# define SI7210_RANGE 0
#endif

/** The interrupt threshold for SI7210 (see act_si7210.h for definition).
 */
#ifdef MBED_CONF_APP_SI7210_INTERRUPT_THRESHOLD_TESLAX1000
# define SI7210_INTERRUPT_THRESHOLD_TESLAX1000 MBED_CONF_APP_SI7210_INTERRUPT_THRESHOLD_TESLAX1000
#else
# define SI7210_INTERRUPT_THRESHOLD_TESLAX1000 1000
#endif

/** The interrupt threshold for SI7210 (see act_si7210.h for definition).
 */
#ifdef MBED_CONF_APP_SI7210_INTERRUPT_HYSTERESIS_TESLAX1000
# define SI7210_INTERRUPT_HYSTERESIS_TESLAX1000 MBED_CONF_APP_SI7210_INTERRUPT_HYSTERESIS_TESLAX1000
#else
# define SI7210_INTERRUPT_HYSTERESIS_TESLAX1000 100
#endif

/** The active high sense for the SI7210 interrupt (see act_si7210.h
 * for definition).
 */
#ifdef MBED_CONF_APP_SI7210_ACTIVE_HIGH
# define SI7210_ACTIVE_HIGH MBED_CONF_APP_SI7210_ACTIVE_HIGH
#else
# define SI7210_ACTIVE_HIGH true
#endif

/**************************************************************************
 * MANIFEST CONSTANTS: BLE
 * Note: most of these taken from
 * https://github.com/u-blox/blueprint-B200-NINA-B1/blob/master/Firmware/src/services/uuids.h
 *************************************************************************/

/** The prefix of wanted BLE devices.
 */
#ifdef MBED_CONF_APP_BLE_PEER_DEVICE_NAME_PREFIX
# define BLE_PEER_DEVICE_NAME_PREFIX MBED_CONF_APP_BLE_PEER_DEVICE_NAME_PREFIX
#else
# define BLE_PEER_DEVICE_NAME_PREFIX "NINA-B1"
#endif

/** The number of data items to retain per device.
 */
#ifdef MBED_CONF_APP_BLE_PEER_NUM_DATA_ITEMS
# define BLE_PEER_NUM_DATA_ITEMS MBED_CONF_APP_BLE_PEER_NUM_DATA_ITEMS
#else
# define BLE_PEER_NUM_DATA_ITEMS 2
#endif

/** The duration of BLE activity.
 */
#ifdef MBED_CONF_APP_BLE_ACTIVE_TIME_MS
# define BLE_ACTIVE_TIME_MS MBED_CONF_APP_BLE_ACTIVE_TIME_MS
#else
# define BLE_ACTIVE_TIME_MS 30000
#endif

/** Allow BLE to be disabled if necessary.
 */
#if defined (MBED_CONF_APP_ENABLE_BLE) && (MBED_CONF_APP_ENABLE_BLE == false)
# define DISABLE_BLE
#endif

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
