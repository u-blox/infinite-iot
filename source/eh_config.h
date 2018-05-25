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

#endif // _EH_CONFIG_H_

// End Of File
