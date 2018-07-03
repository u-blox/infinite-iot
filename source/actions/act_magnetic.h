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

#ifndef _ACT_MAGNETIC_H_
#define _ACT_MAGNETIC_H_

#include <act_common.h>

/**************************************************************************
 * MANIFEST CONSTANTS
 *************************************************************************/

/**************************************************************************
 * TYPES
 *************************************************************************/

/** Possible (bipolar) measurement range settings.
 */
typedef enum {
    RANGE_20_MILLI_TESLAS,
    RANGE_200_MILLI_TESLAS
} FieldStrengthRange;

/**************************************************************************
 * FUNCTIONS
 *************************************************************************/

/** Get the field strength.
 *
 * @param pTeslaX1000   a pointer to a place to put the field strength
 *                      (in milli-Tesla).
 * @return              zero on success or negative error code on failure.
 */
ActionDriver getFieldStrength(unsigned int *pTeslaX1000);

/** Set the measurement range (default is RANGE_20_MILLI_TESLAS).
 *
 * @param range the field strength range.
 * @return      zero on success or negative error code on failure.
 */
ActionDriver setRange(FieldStrengthRange range);

/** Get the measurement range.
 *
 * @return the field strength range.
 */
FieldStrengthRange getRange();

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
ActionDriver setInterrupt(unsigned int thresholdTeslaX1000,
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
ActionDriver getInterrupt(unsigned int *pThresholdTeslaX1000,
                          unsigned int *pHysteresisTeslaX1000,
                          bool *pActiveHigh);

#endif // _ACT_MAGNETIC_H_

// End Of File
