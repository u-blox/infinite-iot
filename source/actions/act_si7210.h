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

#ifndef _ACT_SI7210_H_
#define _ACT_SI7210_H_

#include <act_common.h>

/**************************************************************************
 * MANIFEST CONSTANTS
 *************************************************************************/

/** Default I2C address for the devices Si7210-B-00-IV(R)/Si7210-B-01-IV(R).
 */
#define SI7210_DEFAULT_ADDRESS_00_01 (0x30)

/** Default I2C address for the device Si7210-B-02-IV(R).
 */
#define SI7210_DEFAULT_ADDRESS_02 (0x31)

/** Default I2C address for the device Si7210-B-03-IV(R).
 */
#define SI7210_DEFAULT_ADDRESS_03 (0x32)

/** Default I2C address for the devices Si7210-B-04-IV(R)/Si7210-B-05-IV(R).
 */
#define SI7210_DEFAULT_ADDRESS_04_05 (0x33)

/** The amount of time to wait for the first measurement after initialisation.
 */
#define SI7210_WAIT_FOR_FIRST_MEASUREMENT_MS 1000

/** The amount of time to wait for an item of OTP data to be read.
 */
#define SI7210_WAIT_FOR_OTP_DATA_MS 1000

/** The power consumed, in nanoWatts, while the device is off.
 */
#define SI7210_POWER_OFF_NW 0

/** The power consumed, in nanoWatts, while the device is
 * on and taking measurements every 200 mS, returning
 * to sleep between measurements (0.4 uA @ 3.3V from
 * Table 1.2 of the datasheet).
 */
#define SI7210_POWER_IDLE_NW 1320

/** The energy consumed, in nWh, while the device
 * is performing a reading (nothing, readings are
 * made periodically while idle).
 */
#define SI7210_ENERGY_READING_NWH 0

/**************************************************************************
 * TYPES
 *************************************************************************/

/** Possible (bipolar) measurement range settings.
 */
typedef enum {
    SI7210_RANGE_20_MILLI_TESLAS = 0,
    SI7210_RANGE_200_MILLI_TESLAS = 1
} Si7210FieldStrengthRange;

/**************************************************************************
 * FUNCTIONS
 *************************************************************************/

/** Initialise the hall effect sensor SI7210.
 * Calling this when the SI7210 is already initialised has no effect.
 *
 * @param i2cAddress the address of the SI7210 device
 * @return           zero on success or negative error code on failure.
 */
ActionDriver si7210Init(char i2cAddress);

/** Shutdown the hall effect sensor SI7210.
 * Calling this when the SI7210 has not been initialised has no effect.
 */
void si7210Deinit();

/** Set the measurement range (default is RANGE_20_MILLI_TESLAS).
 * Note: if the range is changed while an interrupt setting is active
 * the interrupt setting will be recalculated to be correct and within
 * the limits of the new range.
 *
 * @param range the field strength range.
 * @return      zero on success or negative error code on failure.
 */
ActionDriver si7210SetRange(Si7210FieldStrengthRange range);

/** Get the measurement range.
 *
 * @return the field strength range.
 */
Si7210FieldStrengthRange si7210GetRange();

/** Set the threshold at which an interrupt from the measuring
 * device will be triggered.  The trigger point of the interrupt
 * is the threshold plus or minus the hysteresis.
 *
 * For the SI7210 device, the ranges are as follows:
 *
 * - threshold can be 0 or it can 80 to 19200 for the
 *   20 milli-Tesla range (x10 for the 200 milli-Tesla range),
 * - If threshold is 0 then hysteresis can be 0 or it can be
 *   80 to 17920 for the 20 milli-Tesla range (x10 for the
 *   200 milli-Tesla range),
 * - if threshold is non-zero hysteresis can be 0 or it can be
 *   40 to 8960 for the 20 milli-Tesla range (x10 for the
 *   200 milli-Tesla range).
 *
 * Rounding may occur when the value is programmed into the device
 * registers so, if you are concerned about accuracy, check the
 * values read back with getInterrupt().
 *
 * @param thresholdTeslaX1000  the threshold in milli-Tesla.
 * @param hysteresisTeslaX1000 the hysteresis on the threshold
 *                             in milli-Tesla.
 * @param activeHigh           if true then the interrupt will go high
 *                             when the threshold is reached, otherwise
 *                             it will go low.
 * @return                     zero on success or negative error code
 *                             on failure.
 */
ActionDriver si7210SetInterrupt(unsigned int thresholdTeslaX1000,
                                unsigned int hysteresisTeslaX1000,
                                bool activeHigh);

/** Get the interrupt settings.
 *
 * @param pThresholdTeslaX1000  pointer to a place to put the threshold.
 * @param pHysteresisTeslaX1000 pointer to a place to put the hysteresis.
 * @param pActiveHigh           pointer to a place to put the active level.
 * @return                      zero on success or negative error code
 *                              on failure.
 */
ActionDriver si7210GetInterrupt(unsigned int *pThresholdTeslaX1000,
                                unsigned int *pHysteresisTeslaX1000,
                                bool *pActiveHigh);

#endif // _ACT_SI7210_H_

// End Of File
