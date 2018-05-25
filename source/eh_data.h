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

#ifndef _EH_DATA_H_
#define _EH_DATA_H_

#include <time.h>
#include <eh_action.h>

/**************************************************************************
 * MANIFEST CONSTANTS
 *************************************************************************/

/**************************************************************************
 * TYPES
 *************************************************************************/

/** The types of data.
 */
typedef enum {
    DATA_TYPE_CELLULAR,
    DATA_TYPE_HUMIDITY,
    DATA_TYPE_ATMOSPHERIC_PRESSURE,
    DATA_TYPE_TEMPERATURE,
    DATA_TYPE_LIGHT,
    DATA_TYPE_ORIENTATION,
    DATA_TYPE_POSITION,
    DATA_TYPE_MAGNETIC,
    DATA_TYPE_BLE,
    DATA_TYPE_WAKE_UP_REASON,
    DATA_TYPE_ENERGY_SOURCE,
    DATA_TYPE_STATISTICS,
    MAX_NUM_DATA_TYPES
} DataType;

/** Definition of a data item.
 */
typedef struct {
    DataType type;
    time_t timeUTC;
    Action *pAction;
    void *pContents;
} Data;

/**************************************************************************
 * FUNCTIONS
 *************************************************************************/

#endif // _EH_DATA_H_

// End Of File
