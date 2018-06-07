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

#ifndef _ACT_TEMPERATURE_HUMIDITY_PRESSURE_H_
#define _ACT_TEMPERATURE_HUMIDITY_PRESSURE_H_

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

/** Get the humidity.
 *
 * @param pPercentage a pointer to a place to put the humidity reading
 *                    (as a percentage).
 * @return            zero on success or negative error code on failure.
 */
ActionDriver getHumidity(unsigned char *pPercentage);

/** Get the atmospheric pressure.
 *
 * @param pPascal a pointer to a place to put the atmospheric pressure reading
 *                (in units of 100ths of a Pascal).
 * @return        zero on success or negative error code on failure.
 */
ActionDriver getPressure(unsigned int *pPascalX100);

/** Get the temperature.
 *
 * @param pTemperatureCx10 a pointer to a place to put the temperature reading
 *                         (in units of 1/100th of a degree Celsius).
 * @return                 zero on success or negative error code on failure.
 */
ActionDriver getTemperature(signed int *pTemperatureCX100);

#endif // _ACT_TEMPERATURE_HUMIDITY_PRESSURE_H_

// End Of File
