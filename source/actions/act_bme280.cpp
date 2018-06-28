/* The code here is borrowed from:
 *
 * https://os.mbed.com/users/MACRUM/code/BME280/#c1f1647004c4
 *
 * All rights remain with the original author(s).
 */

#include <mbed.h>
#include <eh_debug.h>
#include <eh_utilities.h> // for LOCK()/UNLOCK()
#include <eh_i2c.h>
#include <act_temperature_humidity_pressure.h>
#include <act_bme280.h>

/**************************************************************************
 * MANIFEST CONSTANTS
 *************************************************************************/

/**************************************************************************
 * LOCAL VARIABLES
 *************************************************************************/

/** Flag so that we know if we've been initialised.
 */
static bool gInitialised = false;

/** The I2C address of the BME280.
 */
static char gI2cAddress = 0;

/** Mutex to protect the against multiple accessors.
 */
static Mutex gMtx;

/** Various variables required for the calculations
 * (see https://os.mbed.com/users/MACRUM/code/BME280/#c1f1647004c4).
 */
static unsigned short   gDigT1;
static signed short     gDigT2;
static signed short     gDigT3;
static unsigned short   gDigP1;
static signed short     gDigP2;
static signed short     gDigP3;
static signed short     gDigP4;
static signed short     gDigP5;
static signed short     gDigP6;
static signed short     gDigP7;
static signed short     gDigP8;
static signed short     gDigP9;
static unsigned short   gDigH1;
static unsigned short   gDigH3;
static signed short     gDigH2;
static signed short     gDigH4;
static signed short     gDigH5;
static signed short     gDigH6;
static int              gTFine;

/**************************************************************************
 * STATIC FUNCTIONS
 *************************************************************************/

// Get the temperature from the BME280.
// Note: this does not lock the mutex or check for initialisation.
static ActionDriver _getTemperature(int *pTemperature)
{
    ActionDriver result = ACTION_DRIVER_ERROR_NOT_INITIALISED;
    unsigned int temperatureRaw;
    char data[4];

    data[0] = 0xfa; // temp_msb (3 bytes)
    if (i2cSendReceive(gI2cAddress, data, 1, &(data[1]), 3) == 3) {

        temperatureRaw = (data[1] << 12) | (data[2] << 4) | (data[3] >> 4);

        *pTemperature =
        (((((temperatureRaw >> 3) - (gDigT1 << 1))) * gDigT2) >> 11) +
        ((((((temperatureRaw >> 4) - gDigT1) * ((temperatureRaw >> 4) - gDigT1)) >> 12) * gDigT3) >> 14);

        gTFine = *pTemperature;

        *pTemperature = (*pTemperature * 5 + 128) >> 8;

        result = ACTION_DRIVER_OK;

    } else {
        result = ACTION_DRIVER_ERROR_I2C_WRITE_READ;
    }

    return result;
}

/**************************************************************************
 * PUBLIC FUNCTIONS
 *************************************************************************/

// Initialise the humidity/temperature/pressure sensor BME280.
ActionDriver bme280Init(char i2cAddress)
{
    ActionDriver result;
    char data[18];

    LOCK(gMtx);

    result = ACTION_DRIVER_OK;

    if (!gInitialised) {
        gI2cAddress = i2cAddress;
        gTFine = 0;

        data[0] = 0xf2; // ctrl_hum
        data[1] = 0x01; // Humidity over-sampling x1
        if (i2cSendReceive(gI2cAddress, data, 2, NULL, 0) < 0) {
            result = ACTION_DRIVER_ERROR_I2C_WRITE;
        }

        data[0] = 0xf4; // ctrl_meas
        data[1] = 0x27; // Temperature over-sampling x1, Pressure over-sampling x1, Normal mode
        if ((result == ACTION_DRIVER_OK) &&
            (i2cSendReceive(gI2cAddress, data, 2, NULL, 0) < 0)) {
            result = ACTION_DRIVER_ERROR_I2C_WRITE;
        }

        data[0] = 0xf5; // config
        data[1] = 0xa0; // Standby 1000 ms, filter off
        if ((result == ACTION_DRIVER_OK) &&
            (i2cSendReceive(gI2cAddress, data, 2, NULL, 0) < 0)) {
            result = ACTION_DRIVER_ERROR_I2C_WRITE;
        }

        data[0] = 0x88; // read dig_T regs (6 bytes)
        if (result == ACTION_DRIVER_OK) {
            if (i2cSendReceive(gI2cAddress, data, 1, data, 6) == 6) {
                gDigT1 = (data[1] << 8) | data[0];
                gDigT2 = (data[3] << 8) | data[2];
                gDigT3 = (data[5] << 8) | data[4];
            } else {
                result = ACTION_DRIVER_ERROR_I2C_WRITE_READ;
            }
        }

        data[0] = 0x8E; // read dig_P regs (18 bytes)
        if (result == ACTION_DRIVER_OK) {
            if (i2cSendReceive(gI2cAddress, data, 1, data, 18) == 18) {
                gDigP1 = (data[ 1] << 8) | data[ 0];
                gDigP2 = (data[ 3] << 8) | data[ 2];
                gDigP3 = (data[ 5] << 8) | data[ 4];
                gDigP4 = (data[ 7] << 8) | data[ 6];
                gDigP5 = (data[ 9] << 8) | data[ 8];
                gDigP6 = (data[11] << 8) | data[10];
                gDigP7 = (data[13] << 8) | data[12];
                gDigP8 = (data[15] << 8) | data[14];
                gDigP9 = (data[17] << 8) | data[16];
            } else {
                result = ACTION_DRIVER_ERROR_I2C_WRITE_READ;
            }
        }

        data[0] = 0xA1; // read dig_H regs (1 byte)
        if (result == ACTION_DRIVER_OK) {
            if (i2cSendReceive(gI2cAddress, data, 1, data, 1) == 1) {
                data[1] = 0xE1; // read dig_H regs to follow this in data[] (another 7 bytes)
                if (i2cSendReceive(gI2cAddress, &(data[1]), 1, &(data[1]), 7) == 7) {
                    gDigH1 = data[0];
                    gDigH2 = (data[2] << 8) | data[1];
                    gDigH3 = data[3];
                    gDigH4 = (data[4] << 4) | (data[5] & 0x0f);
                    gDigH5 = (data[6] << 4) | ((data[5]>>4) & 0x0f);
                    gDigH6 = data[7];
                } else {
                    result = ACTION_DRIVER_ERROR_I2C_WRITE_READ;
                }
            } else {
                result = ACTION_DRIVER_ERROR_I2C_WRITE_READ;
            }
        }

        gInitialised = (result == ACTION_DRIVER_OK);
        if (!gInitialised) {
            LOG(EVENT_BME280_ERROR, result);
        }
    }

    UNLOCK(gMtx);

    return result;
}

// Shut-down the humidity/temperature/pressure sensor BME280.
void bme280Deinit()
{
    // TODO
    gInitialised = false;
}

// Get the temperature from the BME280.
ActionDriver getTemperature(signed int *pCX100)
{
    ActionDriver result;
    int temperature;

    LOCK(gMtx);

    result = ACTION_DRIVER_ERROR_NOT_INITIALISED;

    if (gInitialised) {
        result = _getTemperature(&temperature);

        if ((result == ACTION_DRIVER_OK) && (pCX100 != NULL)) {
            // temperature is in 100ths of a degree
            *pCX100 = (signed int) temperature;
        }
    }

    if (result != ACTION_DRIVER_OK) {
        LOG(EVENT_BME280_ERROR, result);
    }

    UNLOCK(gMtx);

    return result;
}

// Get the humidity from the BME280.
ActionDriver getHumidity(unsigned char *pPercentage)
{
    ActionDriver result;
    unsigned int humidityRaw;
    int vX1;
    char data[4];

    LOCK(gMtx);

    result = ACTION_DRIVER_ERROR_NOT_INITIALISED;

    if (gInitialised) {
        // Need to have a recent temperature reading if gTFine is
        // to be up to date
        result = _getTemperature(NULL);
        if (result == ACTION_DRIVER_OK) {
            data[0] = 0xfd; // hum_msb (2 bytes)
            if (i2cSendReceive(gI2cAddress, data, 1, &(data[1]), 2) == 2) {

                humidityRaw = (data[1] << 8) | data[2];

                vX1 = gTFine - 76800;
                vX1 =  (((((humidityRaw << 14) -(((int) gDigH4) << 20) - (((int) gDigH5) * vX1)) +
                           ((int) 16384)) >> 15) * (((((((vX1 * (int) gDigH6) >> 10) *
                                                        (((vX1 * ((int) gDigH3)) >> 11) + 32768)) >> 10) + 2097152) *
                                                        (int) gDigH2 + 8192) >> 14));
                vX1 = (vX1 - (((((vX1 >> 15) * (vX1 >> 15)) >> 7) * (int) gDigH1) >> 4));
                vX1 = (vX1 < 0 ? 0 : vX1);
                vX1 = (vX1 > 419430400 ? 419430400 : vX1);
                if (pPercentage != NULL) {
                    *pPercentage = (unsigned char) ((vX1 >> 12) / 1024);
                }

                result = ACTION_DRIVER_OK;

            } else {
                result = ACTION_DRIVER_ERROR_I2C_WRITE_READ;
            }
        }
    }

    if (result != ACTION_DRIVER_OK) {
        LOG(EVENT_BME280_ERROR, result);
    }

    UNLOCK(gMtx);

    return result;
}

// Get the pressure from the BME280.
ActionDriver getPressure(unsigned int *pPascalX100)
{
    ActionDriver result;
    unsigned int pressureRaw;
    int var1;
    int var2;
    unsigned int pressure;
    char data[4];

    LOCK(gMtx);

    result = ACTION_DRIVER_ERROR_NOT_INITIALISED;

    if (gInitialised) {
        // Need to have a recent temperature reading if gTFine is
        // to be up to date
        result = _getTemperature(NULL);
        if (result == ACTION_DRIVER_OK) {
            data[0] = 0xf7; // press_msb (3 bytes)
            if (i2cSendReceive(gI2cAddress, data, 1, &(data[1]), 3) == 3) {

                pressureRaw = (data[1] << 12) | (data[2] << 4) | (data[3] >> 4);

                var1 = (gTFine >> 1) - 64000;
                var2 = (((var1 >> 2) * (var1 >> 2)) >> 11) * gDigP6;
                var2 = var2 + ((var1 * gDigP5) << 1);
                var2 = (var2 >> 2) + (gDigP4 << 16);
                var1 = (((gDigP3 * (((var1 >> 2)*(var1 >> 2)) >> 13)) >> 3) + ((gDigP2 * var1) >> 1)) >> 18;
                var1 = ((32768 + var1) * gDigP1) >> 15;

                if (var1 != 0) {

                    pressure = (((1048576 - pressureRaw) - (var2 >> 12))) * 3125;
                    if (pressure < 0x80000000) {
                        pressure = (pressure << 1) / var1;
                    } else {
                        pressure = (pressure / var1) * 2;
                    }

                    var1 = ((int) gDigP9 * ((int) (((pressure >> 3) * (pressure >> 3)) >> 13))) >> 12;
                    var2 = (((int) (pressure >> 2)) * (int) gDigP8) >> 13;
                    pressure = (pressure + ((var1 + var2 + gDigP7) >> 4));

                    if (pPascalX100 != NULL) {
                        // temperature is hecto-Pascals
                        *pPascalX100 = (unsigned int) pressure;
                    }

                    result = ACTION_DRIVER_OK;

                } else {
                    result = ACTION_DRIVER_ERROR_CALCULATION;
                }
            }
        } else {
            result = ACTION_DRIVER_ERROR_I2C_WRITE_READ;
        }
    }

    if (result != ACTION_DRIVER_OK) {
        LOG(EVENT_BME280_ERROR, result);
    }

    UNLOCK(gMtx);

    return result;
}

// End of file
