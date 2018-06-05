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

#ifndef _ACT_BME280_H_
#define _ACT_BME280_H_

/**************************************************************************
 * MANIFEST CONSTANTS
 *************************************************************************/

#define BME280_DEFAULT_ADDRESS (0x76)

/**************************************************************************
 * TYPES
 *************************************************************************/

/** The return value of bme280 interface calls.
 */
typedef enum {
    BME280_RESULT_OK = 0,
    BME280_RESULT_ERROR_GENERAL = -1,
    BME280_RESULT_ERROR_NOT_INITIALISED = -2,
    BME280_RESULT_ERROR_I2C_WRITE = -3,
    BME280_RESULT_ERROR_I2C_WRITE_READ = -4,
    BME280_RESULT_ERROR_PRESSURE_CALCULATION = -5
} Bme280Result;

/**************************************************************************
 * FUNCTIONS
 *************************************************************************/

/** Initialise the humidity/temperature/pressure sensor BME280.
 *
 * @param i2cAddress the address of the BME280 device
 * @return           zero on success or negative error code on failure.
 */
Bme280Result bme280Init(char i2cAddress);

/** Shutdown the humidity/temperature/pressure sensor BME280.
 */
void bme280Deinit();

/** Get the humidity from the BME280.
 *
 * @param pPercentage a pointer to a place to put the humidity reading
 *                    (as a percentage).
 * @return            zero on success or negative error code on failure.
 */
Bme280Result getHumidity(unsigned char *pPercentage);

/** Get the atmospheric pressure from the BME280.
 *
 * @param pPascal a pointer to a place to put the atmospheric pressure reading
 *                (in units of 100ths of a Pascal).
 * @return        zero on success or negative error code on failure.
 */
Bme280Result getPressure(unsigned int *pPascalX100);

/** Get the temperature from the BME280.
 *
 * @param pTemperatureCx10 a pointer to a place to put the temperature reading
 *                         (in units of 1/100th of a degree Celsius).
 * @return                 zero on success or negative error code on failure.
 */
Bme280Result getTemperature(signed int *pTemperatureCX100);

#endif // _ACT_BME280_H_

// End Of File
