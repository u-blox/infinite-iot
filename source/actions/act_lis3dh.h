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

#ifndef _ACT_LIS3DH_H_
#define _ACT_LIS3DH_H_

#include <act_common.h>

/**************************************************************************
 * MANIFEST CONSTANTS
 *************************************************************************/

/** Default I2C address when pin SA0 is grounded.
 */
#define LIS3DH_DEFAULT_ADDRESS_SA0_GND (0x18)

/** Default I2C address when pin SA0 is at VSupply.
 */
#define LIS3DH_DEFAULT_ADDRESS_SA0_VSUPPLY (0x19)

/** The power consumed, in nanoWatts, while the device is
 * off (0.5 uA @ 1.8V from Table 6 of the datasheet).
 */
#define LIS3DH_ENERGY_OFF_NW 900

/** The power consumed, in nanoWatts, while the device is
 * on and idle (2 uA @ 1.8V from Table 12 of the
 * datasheet).
 */
#define LIS3DH_POWER_IDLE_NW 3600

/** The energy consumed, in nWh, while the device
 * is performing a reading (nothing, readings are
 * made periodically while idle).
 */
#define LIS3DH_ENERGY_READING_NWH 0

/**************************************************************************
 * TYPES
 *************************************************************************/

/**************************************************************************
 * FUNCTIONS
 *************************************************************************/

/** Initialise the LIS3DH accelerometer.
 * Calling this when the LIS3DH is already initialised has no effect.
 *
 * @param i2cAddress the address of the LIS3DH device
 * @return           zero on success or negative error code on failure.
 */
ActionDriver lis3dhInit(char i2cAddress);

/** Shutdown the LIS3DH accelerometer.
 * Calling this when the LIS3DH has not been initialised has no effect.
 */
void lis3dhDeinit();

/** Set the sensitivity of the device.
 * Note: if the sensitivity is changed while an interrupt setting is active
 * the interrupt setting will be recalculated to be correct and within
 * the limits of the new sensitivity rage.
 *
 * For the LIS3DH device the sensitivity values are coded
 * as follows.
 *
 * 0: full scale +/- 2 g
 * 1: full scale +/- 4 g
 * 2: full scale +/- 8 g
 * 3: full scale +/- 16 g
 *
 * For instance, with a sensitivity of 0, +2g of acceleration
 * is represented by the value +32768 (since the output is max
 * 16-bits) and -2g of acceleration is represented by the value of
 * -32767 and so each bit is 60 mg.
 *
 * @param sensitivity  the sensitivity.
 * @return             zero on success or negative error code
 *                     on failure.
 */
ActionDriver lis3dhSetSensitivity(unsigned char sensitivity);

/** Get the sensitivity of the device.
 *
 * @param pSensitivity pointer to a place to store the
 *                     sensitivity.
 * @return             zero on success or negative error code
 *                     on failure.
 */
ActionDriver lis3dhGetSensitivity(unsigned char *pSensitivity);

/** Set the threshold of motion which will cause
 * an interrupt and the interrupt number to use.
 * Note: this does NOT enable the interrupt,
 * see enableInterrupt() for that.
 *
 * For the LIS3DH device the value in milli-G is converted
 * internally with a resolution of 7 bits, scaled
 * according to the sensitivity.  For instance, with
 * a full scale of +/- 2G, one LSB is 16 milli-G.
 *
 * @param interrupt   the interrupt pin to use; on the LIS3DH
 *                    device this can be 1 or 2.
 * @param threshold   the threshold in milli-G.
 * @return            zero on success or negative error code
 *                    on failure.
 */
ActionDriver lis3dhSetInterruptThreshold(unsigned char interrupt,
                                         unsigned int thresholdMG);

/** Get the interrupt threshold for an interrupt pin.
 *
 * @param interrupt    the interrupt pin to query; on the LIS3DH
 *                     device this can be 1 or 2.
 * @param *pThreshold  pointer to a place to store the
 *                     threshold that is in use on that pin.
 * @return             zero on success or negative error code
 *                     on failure.
 */
ActionDriver lis3dhGetInterruptThreshold(unsigned char interrupt,
                                         unsigned int *pThresholdMG);

/** Enable or disable the given interrupt.
 *
 * @param interrupt        the interrupt pin; on the LIS3DH device
 *                         this can be 1 or 2.
 * @param enableNotDisable true to enable the interrupt, false to
 *                         disable the interrupt.
 * @param pEventQueue      event queue to use when interrupt goes off
 *                         (ignored if enableNotDisable is false).
 * @param pEventCallback   event callback to be called on the event
 *                         queue when the interrupt goes off
 *                         (ignored if enableNotDisable is false).
 * @return                 zero on success or negative error code
 *                         on failure.
 */
ActionDriver lis3dhSetInterruptEnable(unsigned char interrupt,
                                      bool enableNotDisable,
                                      EventQueue *pEventQueue,
                                      void (*pEventCallback) (EventQueue *));

/** Get the state of the given interrupt.
 *
 * @param interrupt           the interrupt pin; on the LIS3DH
 *                            device this can be 1 or 2.
 * @param *pEnableNotDisable  pointer to a place to store the
 *                            interrupt state.
 * @return                    zero on success or negative error
 *                            code on failure.
 */
ActionDriver lis3dhGetInterruptEnable(unsigned char interrupt,
                                      bool *pEnableNotDisable);

/** Clear the interrupt. MUST be called to reset the interrupt
 * pin after an interrupt has gone off.
 *
 * @param interrupt the interrupt pin to clear; on the LIS3DH
 *                  device this can be 1 or 2.
 * @return          zero on success or negative error
 *                  code on failure; in particular, if the
 *                  interrupt is not set when this is called
 *                  then ACTION_DRIVER_ERROR_NO_INTERRUPT is
 *                  returned.
 */
ActionDriver lis3dhClearInterrupt(unsigned char interrupt);

#endif // _ACT_LIS3DH_H_

// End Of File
