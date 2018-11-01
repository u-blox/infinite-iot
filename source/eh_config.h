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

/** How frequently to update time (as a maximum).
 */
#define TIME_UPDATE_INTERVAL_SECONDS (24 * 3600)

/** The default energy source (1, 2 or 3, can't be 0).
 */
#ifdef MBED_CONF_APP_ENERGY_SOURCE_DEFAULT
# define ENERGY_SOURCE_DEFAULT MBED_CONF_APP_ENERGY_SOURCE_DEFAULT
#else
# define ENERGY_SOURCE_DEFAULT 3
#endif

/** The percentage of the data queue at which we must send out
 * a report.
 */
#ifdef MBED_CONF_APP_MAX_DATA_QUEUE_LENGTH_PERCENT
# define MAX_DATA_QUEUE_LENGTH_PERCENT MBED_CONF_APP_MAX_DATA_QUEUE_LENGTH_PERCENT
#else
# define MAX_DATA_QUEUE_LENGTH_PERCENT 90
#endif

/** If logging is enabled and it's not only printed-out
 * logging, it's being reporting over the air, then we
 * have to report every wake-up so as to avoid a
 * logging buffer overrun.
 */
#if defined(MBED_CONF_APP_ENABLE_LOGGING) && \
     MBED_CONF_APP_ENABLE_LOGGING && \
     !(defined (MBED_CONF_APP_LOG_PRINT_ONLY) && \
       MBED_CONF_APP_LOG_PRINT_ONLY)
# define LOGGING_NEEDS_REPORTING_EACH_WAKEUP 1
#else
# define LOGGING_NEEDS_REPORTING_EACH_WAKEUP 0
#endif

/** As we're running in a small memory space
 * and there is a risk of data being lost (and
 * hence kept in the data buffer) then fragmentation
 * may be in issue so, if the data is all of the
 * the same importance etc. then set
 * AVOID_FRAGMENTATION to 1 and, instead of
 * sorting the data, it will be sent in the order
 * it was allocated.
 */
#ifdef MBED_CONF_APP_AVOID_FRAGMENTATION
# define AVOID_FRAGMENTATION MBED_CONF_APP_AVOID_FRAGMENTATION
#else
# define AVOID_FRAGMENTATION 1
#endif

/**************************************************************************
 * MANIFEST CONSTANTS: DEBUG
 *************************************************************************/

/** Whether the modem drive should spit out debug prints.
 */
#ifdef MBED_CONF_APP_MODEM_DEBUG
# define MODEM_DEBUG MBED_CONF_APP_MODEM_DEBUG
#else
# define MODEM_DEBUG 0
#endif

/** Ignore VBAT_OK state: useful when running from a power supply.
 */
#ifdef MBED_CONF_APP_IGNORE_BATTERY_STATE
# define IGNORE_BATTERY_STATE MBED_CONF_APP_IGNORE_BATTERY_STATE
#else
# define IGNORE_BATTERY_STATE 0
#endif

/**************************************************************************
 * MANIFEST CONSTANTS: TIMINGS
 *************************************************************************/

/** How frequently to wake-up to see if there is enough energy
 * to do anything.
 * Note: if the wake up interval is greater than 71 minutes (0xFFFFFFFF
 * microseconds) then the logging system will be unable to tell if the
 * logging timestamp has wrapped.  Not a problem for the main application
 * but may affect your view of the debug logs sent to the server.
 */
#ifdef MBED_CONF_APP_WAKEUP_INTERVAL_SECONDS
# define WAKEUP_INTERVAL_SECONDS MBED_CONF_APP_WAKEUP_INTERVAL_SECONDS
#else
# define WAKEUP_INTERVAL_SECONDS 120
#endif

/** The maximum run-time of the processor; should be less than the wake-up
 * interval otherwise we will skip wake-up intervals (we won't run a new
 * one when the previous one is still running).
 */
#ifdef MBED_CONF_APP_MAX_RUN_TIME_SECONDS
# define MAX_RUN_TIME_SECONDS  MBED_CONF_APP_MAX_RUN_TIME_SECONDS
#else
# define MAX_RUN_TIME_SECONDS  90
#endif

/** The maximum run-time of the processor when the modem is not
 * known to have registered before.  Modems can take a long time
 * to register when they don't know where they are.  Once they've
 * sorted themselves out they should register much more quickly.
 */
#ifdef MBED_CONF_APP_MAX_RUN_FIRST_TIME_SECONDS
# define MAX_RUN_FIRST_TIME_SECONDS  MBED_CONF_APP_MAX_RUN_FIRST_TIME_SECONDS
#else
# define MAX_RUN_FIRST_TIME_SECONDS  (60 * 6)
#endif

/** Watchdog timer duration.  The watchdog is fed only at the start of
 * a wake-up and so the watchdog timer duration must be at least the
 * maximum duration of a wake-up plus the maximum value of the
 * wake-up interval.
 */
#ifdef MBED_CONF_APP_WATCHDOG_INTERVAL_SECONDS
# define WATCHDOG_INTERVAL_SECONDS MBED_CONF_APP_WATCHDOG_INTERVAL_SECONDS
#else
# define WATCHDOG_INTERVAL_SECONDS (MAX_RUN_TIME_SECONDS + WAKEUP_INTERVAL_SECONDS + 30)
#endif

/** The number of seconds for which to keep a history of the energy
 * choices made; must be at least one WAKEUP_INTERVAL_SECONDS.
 */
#ifdef MBED_CONF_APP_ENERGY_HISTORY_SECONDS
# define ENERGY_HISTORY_SECONDS MBED_CONF_APP_ENERGY_HISTORY_SECONDS
#else
# define ENERGY_HISTORY_SECONDS (60 * 30)
#endif

/** The time for which to try GNSS fixing without any
 * back-off on failure.
 */
#ifdef MBED_CONF_APP_LOCATION_FIX_NO_BACK_OFF_SECONDS
# define LOCATION_FIX_NO_BACK_OFF_SECONDS MBED_CONF_APP_LOCATION_FIX_NO_BACK_OFF_SECONDS
#else
# define LOCATION_FIX_NO_BACK_OFF_SECONDS (60 * 2)
#endif

/** The maximum period between location fix attempts, used
 * to limit the back-off algorithm when position attempts keep
 * on failing (and wasting energy); must be at least one
 * WAKEUP_INTERVAL_SECONDS.
 */
#ifdef MBED_CONF_APP_LOCATION_FIX_MAX_PERIOD_SECONDS
# define LOCATION_FIX_MAX_PERIOD_SECONDS MBED_CONF_APP_LOCATION_FIX_MAX_PERIOD_SECONDS
#else
# define LOCATION_FIX_MAX_PERIOD_SECONDS 3600
#endif

/**  Define this to disable location measurement (e.g. if
 * you know you're always going to be indoors).
 */
#ifdef MBED_CONF_APP_ENABLE_LOCATION
# define ENABLE_LOCATION MBED_CONF_APP_ENABLE_LOCATION
#else
# define ENABLE_LOCATION 1
#endif

/** The maximum time between reports (energy permitting,
 * of course), set to 0 for no maximum time.
 */
#ifdef MBED_CONF_APP_MAX_REPORT_INTERVAL_SECONDS
# define MAX_REPORT_INTERVAL_SECONDS  MBED_CONF_APP_MAX_REPORT_INTERVAL_SECONDS
#else
# define MAX_REPORT_INTERVAL_SECONDS  3600
#endif

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
#define APPLICATION_LOG_VERSION 14

/**************************************************************************
 * MANIFEST CONSTANTS: CELLULAR
 *************************************************************************/

/** Define this to force the build into N2-module only mode.
 */
#ifdef MBED_CONF_APP_FORCE_N2_MODEM
# define FORCE_N2_MODEM MBED_CONF_APP_FORCE_N2_MODEM
#else
# define FORCE_N2_MODEM 0
#endif

/** Define this to switch the N211 modem off when not in use (and suffer
 * the registration cost of switching it on again) rather than
 * leaving it in low-power idle.
 */
#ifdef MBED_CONF_APP_CELLULAR_N211_OFF_WHEN_NOT_IN_USE
# define CELLULAR_N211_OFF_WHEN_NOT_IN_USE MBED_CONF_APP_CELLULAR_N211_OFF_WHEN_NOT_IN_USE
#else
# define CELLULAR_N211_OFF_WHEN_NOT_IN_USE 1
#endif

/** Define this to default to North American settings for the
 * R410M module below (i.e. Cat-M1 is the primary RAT with the
 * North American bands). If this is NOT defined then European
 * settings (NBIoT with bands 8 and 20) prevail for R410M.
 */
# if defined(MBED_CONF_APP_CELLULAR_R4_NO_RAT_CHANGE) && \
     MBED_CONF_APP_CELLULAR_R4_NO_RAT_CHANGE
# if FORCE_N2_MODEM
#  error "N2 modem doesn't support Cat-M1, which is required for North America"
# endif
#endif

/** The requested periodic RAU timer in seconds, the interval
 * at which the network agrees that the modem will autonomously
 * wake-up and contact the network simply to confirm it's
 * still there, only relevant for the N211 modem if
 * CELLULAR_N211_OFF_WHEN_NOT_IN_USE is 0.
 */
#ifdef MBED_CONF_APP_CELLULAR_PERIODIC_RAU_TIME_SECONDS
# define CELLULAR_PERIODIC_TAU_TIME_SECONDS  MBED_CONF_APP_CELLULAR_PERIODIC_TAU_TIME_SECONDS
#else
# define CELLULAR_PERIODIC_TAU_TIME_SECONDS (3600 * 24 * 7)
#endif

/** The requested active time in seconds, the time
 * for which the network will keep in contact with the modem
 * immediately after the end of a transmission, only relevant
 * for the N211 modem if CELLULAR_N211_OFF_WHEN_NOT_IN_USE is 0.
 */
#ifdef MBED_CONF_APP_CELLULAR_ACTIVE_TIME_SECONDS
# define CELLULAR_ACTIVE_TIME_SECONDS  MBED_CONF_APP_CELLULAR_ACTIVE_TIME_SECONDS
#else
# define CELLULAR_ACTIVE_TIME_SECONDS 20
#endif

/** The RAT for the R4 modem, chosen from 7
 * (Cat-M1), 8 (NBIoT) or -1 for don't set it, leave
 * the modem at defaults.
 */
#ifdef MBED_CONF_APP_CELLULAR_R4_RAT
# define CELLULAR_R4_RAT  MBED_CONF_APP_CELLULAR_R4_RAT
#else
# if defined(MBED_CONF_APP_CELLULAR_R4_NO_RAT_CHANGE) && \
     MBED_CONF_APP_CELLULAR_R4_NO_RAT_CHANGE
#   define CELLULAR_R4_RAT -1
# else
#  if defined(MBED_CONF_APP_NORTH_AMERICA) && \
      MBED_CONF_APP_NORTH_AMERICA
// Cat M1
#   define CELLULAR_R4_RAT 7
#  else
// NBIoT
#   define CELLULAR_R4_RAT 8
#  endif
# endif
#endif

/** The band mask for the RAT of the R4 modem,
 * a bitmap where bit 0 is band 1 and bit 63 is band 64.
 * Only relevant if CELLULAR_R4_RAT is not -1.
 */
#ifdef MBED_CONF_APP_CELLULAR_R4_BAND_MASK
# define CELLULAR_R4_BAND_MASK  MBED_CONF_APP_CELLULAR_R4_BAND_MASK
#else
# if defined(MBED_CONF_APP_NORTH_AMERICA) && \
     MBED_CONF_APP_NORTH_AMERICA
// The North American bands, for Cat-M1
#  define CELLULAR_R4_BAND_MASK 0x000000400B0F189FLL
# else
// Bands 8 and 20, suitable for NBIoT in Europe
#  define CELLULAR_R4_BAND_MASK 0x0000000000080080LL
# endif
#endif

/** The credentials of the SIM in the board.  If PIN checking
 * is enabled for your SIM card you must set this to the
 * required PIN.
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

/** The socket timeout: keep this short, the
 * APIs are called multiple times based on other
 * timers anyway.
 */
#ifdef MBED_CONF_APP_SOCKET_TIMEOUT_MS
# define SOCKET_TIMEOUT_MS MBED_CONF_APP_SOCKET_TIMEOUT_MS
#else
# define SOCKET_TIMEOUT_MS 0
#endif

/** Whether acks are required for normal data reports or not.
 * Note: I'd love to go without acks, I really would,
 * but if we are powering the module down there is no way to
 * tell whether the SARA-R4 modem has finished sending a report
 * or not before powering it down and waking up and sending
 * from sleep sometimes elicits a series of CME ERROR operation
 * not allowed so acks it is I'm afraid.
 */
#ifdef MBED_CONF_APP_ACK_FOR_REPORTS
# define ACK_FOR_REPORTS  MBED_CONF_APP_ACK_FOR_REPORTS
#else
# define ACK_FOR_REPORTS true
#endif

/** The time to wait for an ack from the server.
 * Make this any smaller than around 3 seconds and
 * you risk missing out on acks.
 */
#ifdef MBED_CONF_APP_ACK_TIMEOUT_MS
# define ACK_TIMEOUT_MS  MBED_CONF_APP_ACK_TIMEOUT_MS
#else
# define ACK_TIMEOUT_MS 3000
#endif

/** A threshold on the number of times a reporting session might
 * fail: if we hit this, need to give the modem a nice rest.
 */
#ifdef MBED_CONF_APP_MAX_NUM_REPORT_FAILURES
# define MAX_NUM_REPORT_FAILURES MBED_CONF_APP_MAX_NUM_REPORT_FAILURES
#else
# define MAX_NUM_REPORT_FAILURES 1
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

/** Output pin to enable 1.8V power to GNSS and to the
 * pull-up resistors on the serial lines to the N211 module.
 */
#ifdef MBED_CONF_APP_PIN_ENABLE_1V8
# define PIN_ENABLE_1V8             MBED_CONF_APP_PIN_ENABLE_1V8
#else
# define PIN_ENABLE_1V8             NINA_B1_GPIO_28
#endif

/** Pin which enables the voltage dividers allowing analogue
 * voltage measurements to be made.
 */
#ifdef MBED_CONF_APP_PIN_ENABLE_VOLTAGE_DIVIDERS
# define PIN_ENABLE_VOLTAGE_DIVIDERS MBED_CONF_APP_PIN_ENABLE_VOLTAGE_DIVIDERS
#else
# define PIN_ENABLE_VOLTAGE_DIVIDERS NINA_B1_GPIO_29
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
