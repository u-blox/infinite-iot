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

#ifndef _EH_POST_H_
#define _EH_POST_H_

/**************************************************************************
 * MANIFEST CONSTANTS
 *************************************************************************/

/**************************************************************************
 * TYPES
 *************************************************************************/

/** The return value of post().
 */
typedef enum {
    POST_RESULT_OK = 0,
    POST_RESULT_ERROR_GENERAL = -1,
    POST_RESULT_ERROR_CELLULAR = -2,
    POST_RESULT_ERROR_BME280 = -3,
    POST_RESULT_ERROR_SI1133 = -4,
    POST_RESULT_ERROR_LIS3DH = -5,
    POST_RESULT_ERROR_SI7210 = -6,
    POST_RESULT_ERROR_GNSS = -7,
    POST_RESULT_ERROR_BLE = -8
} PostResult;

/**************************************************************************
 * FUNCTIONS
 *************************************************************************/

/** Perform a power-on self test.
 *
 * @param  bestEffort  if true then, should a device fail, POST will mark the
 *                     device as "not desirable" so it is ignored and will
 *                     continue for a best effort service.  If false then
 *                     all items must be present to achieve a POST_OK result.
 * @return             the result of the power-on self test, 0 on success,
 *                     negative error code on error.
 */
PostResult post(bool bestEffort);

#endif // _EH_POST_H_

// End Of File
