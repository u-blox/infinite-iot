/* mbed Microcontroller Library
 * Copyright (c) 2006-2018 u-blox Limited
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <mbed.h> // For the pin names
#include <eh_debug.h>
#include <eh_action.h>
#include <eh_i2c.h>
#include <eh_config.h>
#include <act_bme280.h>
#include <act_si1133.h>
#include <act_si7210.h>
#include <act_lis3dh.h>
#include <act_zoem8.h>
#include <eh_post.h>

/**************************************************************************
 * MANIFEST CONSTANTS
 *************************************************************************/

/**************************************************************************
 * LOCAL VARIABLES
 *************************************************************************/

/**************************************************************************
 * STATIC FUNCTIONS
 *************************************************************************/

/**************************************************************************
 * PUBLIC FUNCTIONS
 *************************************************************************/

// Perform a power-on self test.
PostResult post(bool bestEffort)
{
    PostResult result = POST_RESULT_OK;

    // Instantiate I2C
    i2cInit(I2C_SDA0, I2C_SCL0);

    LOG(EVENT_POST_BEST_EFFORT, bestEffort);

    for (unsigned int x = ACTION_TYPE_NULL + 1;
         (x < MAX_NUM_ACTION_TYPES) && (bestEffort || (result == POST_RESULT_OK));
         x++) {
        switch (x) {
            case ACTION_TYPE_REPORT:
                 // TODO
            break;
            case ACTION_TYPE_GET_TIME_AND_REPORT:
                // Nothing to do, all done in ACTION_TYPE_REPORT
            break;
            case ACTION_TYPE_MEASURE_HUMIDITY:
                // Do all of the BME280 humidity/temperature/pressure
                // device in one go here
                if (bme280Init(BME280_DEFAULT_ADDRESS) != ACTION_DRIVER_OK) {
                    result = POST_RESULT_ERROR_BME280;
                    LOG(EVENT_POST_ERROR, result);
                    if (bestEffort) {
                        actionSetDesirability(ACTION_TYPE_MEASURE_HUMIDITY, 0);
                        actionSetDesirability(ACTION_TYPE_MEASURE_ATMOSPHERIC_PRESSURE, 0);
                        actionSetDesirability(ACTION_TYPE_MEASURE_TEMPERATURE, 0);
                    }
                }
                bme280Deinit();
            break;
            case ACTION_TYPE_MEASURE_ATMOSPHERIC_PRESSURE:
                // Nothing to do, all done in ACTION_TYPE_MEASURE_HUMIDITY
            break;
            case ACTION_TYPE_MEASURE_TEMPERATURE:
                // Nothing to do, all done in ACTION_TYPE_MEASURE_HUMIDITY
            break;
            case ACTION_TYPE_MEASURE_LIGHT:
                // Attempt initialisation of the light sensor
                if (si1133Init(SI1133_DEFAULT_ADDRESS) != ACTION_DRIVER_OK) {
                    result = POST_RESULT_ERROR_SI1133;
                    LOG(EVENT_POST_ERROR, result);
                    if (bestEffort) {
                        actionSetDesirability(ACTION_TYPE_MEASURE_LIGHT, 0);
                    }
                }
                si1133Deinit();
            break;
            case ACTION_TYPE_MEASURE_ORIENTATION:
                // Intialise the orientation sensor
                if (lis3dhInit(LIS3DH_DEFAULT_ADDRESS) != ACTION_DRIVER_OK) {
                    result = POST_RESULT_ERROR_LIS3DH;
                    LOG(EVENT_POST_ERROR, result);
                    if (bestEffort) {
                        actionSetDesirability(ACTION_TYPE_MEASURE_ORIENTATION, 0);
                    }
                }
                // Do don't de-initialise this, it can be left on in lowest power state
            break;
            case ACTION_TYPE_MEASURE_POSITION:
                // Attempt instantiation of the GNSS driver
                if (zoem8Init(ZOEM8_DEFAULT_ADDRESS) != ACTION_DRIVER_OK) {
                    result = POST_RESULT_ERROR_ZOEM8;
                    LOG(EVENT_POST_ERROR, result);
                    if (bestEffort) {
                        actionSetDesirability(ACTION_TYPE_MEASURE_POSITION, 0);
                    }
                }
                zoem8Deinit();
            break;
            case ACTION_TYPE_MEASURE_MAGNETIC:
                // Initialise the hall effect sensor
                if (si7210Init(SI7210_DEFAULT_ADDRESS) != ACTION_DRIVER_OK) {
                    result = POST_RESULT_ERROR_SI7210;
                    LOG(EVENT_POST_ERROR, result);
                    if (bestEffort) {
                        actionSetDesirability(ACTION_TYPE_MEASURE_MAGNETIC, 0);
                    }
                }
                // Do don't de-initialise this, it can be left on in lowest power state
            break;
            case ACTION_TYPE_MEASURE_BLE:
                // TODO
            break;
            default:
                MBED_ASSERT(false);
            break;
        }
    }

    // Shut down I2C
    i2cDeinit();

    if (bestEffort) {
        result = POST_RESULT_OK;
        LOG(EVENT_POST_ERROR, result);
    }

    return result;
}

// End of file
