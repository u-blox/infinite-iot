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

#ifndef _ACT_COMMON_H_
#define _ACT_COMMON_H_

/**************************************************************************
 * MANIFEST CONSTANTS
 *************************************************************************/

/**************************************************************************
 * TYPES
 *************************************************************************/

/** The return values for the action drivers.
 */
typedef enum {
    ACTION_DRIVER_OK = 0,
    ACTION_DRIVER_ERROR_GENERAL = -1,
    ACTION_DRIVER_ERROR_NOT_INITIALISED = -2,
    ACTION_DRIVER_ERROR_I2C_WRITE = -3,
    ACTION_DRIVER_ERROR_I2C_WRITE_READ = -4,
    ACTION_DRIVER_ERROR_CALCULATION = -5
} ActionDriver;

#endif // _ACT_COMMON_H_

// End Of File
