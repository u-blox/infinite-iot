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

#ifndef _ACT_ORIENTATION_H_
#define _ACT_ORIENTATION_H_

#include <act_common.h>

/**************************************************************************
 * MANIFEST CONSTANTS
 *************************************************************************/

/**************************************************************************
 * TYPES
 *************************************************************************/

/**************************************************************************
 * FUNCTIONS
 *************************************************************************/

/** Get the orientation in x, y, z coordinates, each signed 16-bit values.
 *
 * @param pX  a place to put the X coordinate.
 * @param pY  a place to put the Y coordinate.
 * @param pZ  a place to put the Z coordinate.
 * @return    zero on success or negative error code on failure.
 */
ActionDriver getOrientation(int *pX, int *pY, int *pZ);

/** Set the sensitivity of the device.
 * Note: if the sensitivity is changed while an interrupt setting is active
 * the interrupt setting will be recalculated to be correct and within
 * the limits of the new sensitivity rage.
 *
 * For the LIS3DH device the sensitivity values are coded
 * as follows.
 *
 * 0: full scale +/- 2 G
 * 1: full scale +/- 4 G
 * 2: full scale +/- 8 G
 * 3: full scale +/- 16 G
 *
 * @param sensitivity  the sensitivity.
 * @return             zero on success or negative error code
 *                     on failure.
 */
ActionDriver setSensitivity(unsigned char sensitivity);

/** Get the sensitivity of the device.
 *
 * @param pSensitivity pointer to a place to store the
 *                     sensitivity.
 * @return             zero on success or negative error code
 *                     on failure.
 */
ActionDriver getSensitivity(unsigned char *pSensitivity);

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
ActionDriver setInterruptThreshold(unsigned char interrupt,
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
ActionDriver getInterruptThreshold(unsigned char interrupt,
                                   unsigned int *pThresholdMG);

/** Enable or disable the given interrupt.
 *
 * @param interrupt        the interrupt pin; on the LIS3DH device
 *                         this can be 1 or 2.
 * @param enableNotDisable true to enable the interrupt, false to
 *                         disable the interrupt.
 * @return                 zero on success or negative error code
 *                         on failure.
 */
ActionDriver setInterruptEnable(unsigned char interrupt,
                                bool enableNotDisable);

/** Get the state of the given interrupt.
 *
 * @param interrupt           the interrupt pin; on the LIS3DH
 *                            device this can be 1 or 2.
 * @param *pEnableNotDisable  pointer to a place to store the
 *                            interrupt state.
 * @return                    zero on success or negative error
 *                            code on failure.
 */
ActionDriver getInterruptEnable(unsigned char interrupt,
                                bool *pEnableNotDisable);

#endif // _ACT_ORIENTATION_H_

// End Of File
