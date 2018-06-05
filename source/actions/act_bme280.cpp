/* The code here is borrowed from:
 *
 * https://os.mbed.com/users/MACRUM/code/BME280/#c1f1647004c4
 */

#include <mbed.h>
#include <eh_debug.h>
#include <eh_i2c.h>
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

/** Various variables required for the calculations
 * (see https://os.mbed.com/users/MACRUM/code/BME280/#c1f1647004c4).
 */
static uint16_t    gDigT1;
static int16_t     gDigT2;
static int16_t     gDigT3;
static uint16_t    gDigP1;
static int16_t     gDigP2;
static int16_t     gDigP3;
static int16_t     gDigP4;
static int16_t     gDigP5;
static int16_t     gDigP6;
static int16_t     gDigP7;
static int16_t     gDigP8;
static int16_t     gDigP9;
static uint16_t    gDigH1;
static uint16_t    gDigH3;
static int16_t     gDigH2;
static int16_t     gDigH4;
static int16_t     gDigH5;
static int16_t     gDigH6;
static int32_t     gTFine;

/**************************************************************************
 * STATIC FUNCTIONS
 *************************************************************************/

/**************************************************************************
 * PUBLIC FUNCTIONS
 *************************************************************************/

// Initialise the humidity/temperature/pressure sensor BME280.
Bme280Result bme280Init(char i2cAddress)
{
    Bme280Result result = BME280_RESULT_OK;
    char data[18];

    gI2cAddress = i2cAddress;
    gTFine = 0;

    data[0] = 0xf2; // ctrl_hum
    data[1] = 0x01; // Humidity over-sampling x1
    if (i2cSendReceive(gI2cAddress, data, 2, NULL, 0) < 0) {
        result = BME280_RESULT_ERROR_I2C_WRITE;
    }

    data[0] = 0xf4; // ctrl_meas
    data[1] = 0x27; // Temperature over-sampling x1, Pressure over-sampling x1, Normal mode
    if ((result == BME280_RESULT_OK) &&
        (i2cSendReceive(gI2cAddress, data, 2, NULL, 0) < 0)) {
        result = BME280_RESULT_ERROR_I2C_WRITE;
    }

    data[0] = 0xf5; // config
    data[1] = 0xa0; // Standby 1000 ms, filter off
    if ((result == BME280_RESULT_OK) &&
        (i2cSendReceive(gI2cAddress, data, 2, NULL, 0) < 0)) {
        result = BME280_RESULT_ERROR_I2C_WRITE;
    }

    data[0] = 0x88; // read dig_T regs (6 bytes)
    if (result == BME280_RESULT_OK) {
        if (i2cSendReceive(gI2cAddress, data, 1, data, 6) == 6) {
            gDigT1 = (data[1] << 8) | data[0];
            gDigT2 = (data[3] << 8) | data[2];
            gDigT3 = (data[5] << 8) | data[4];
            PRINTF("dig_T = 0x%x, 0x%x, 0x%x.\n", gDigT1, gDigT2, gDigT3);
        } else {
            result = BME280_RESULT_ERROR_I2C_WRITE_READ;
        }
    }

    data[0] = 0x8E; // read dig_P regs (18 bytes)
    if (result == BME280_RESULT_OK) {
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
            PRINTF("dig_P = 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x.\n",
                   gDigP1, gDigP2, gDigP3, gDigP4, gDigP5, gDigP6, gDigP7, gDigP8, gDigP9);
        } else {
            result = BME280_RESULT_ERROR_I2C_WRITE_READ;
        }
    }

    data[0] = 0xA1; // read dig_H regs (1 byte)
    if (result == BME280_RESULT_OK) {
        if (i2cSendReceive(gI2cAddress, data, 1, data, 1) == 1) {
            data[1] = 0xE1; // read dig_H regs to follow this in data[] (another 7 bytes)
            if (i2cSendReceive(gI2cAddress, &(data[1]), 1, &(data[1]), 7) == 7) {
                gDigH1 = data[0];
                gDigH2 = (data[2] << 8) | data[1];
                gDigH3 = data[3];
                gDigH4 = (data[4] << 4) | (data[5] & 0x0f);
                gDigH5 = (data[6] << 4) | ((data[5]>>4) & 0x0f);
                gDigH6 = data[7];
                PRINTF("dig_H = 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x.\n",
                       gDigH1, gDigH2, gDigH3, gDigH4, gDigH5, gDigH6);
            } else {
                result = BME280_RESULT_ERROR_I2C_WRITE_READ;
            }
        } else {
            result = BME280_RESULT_ERROR_I2C_WRITE_READ;
        }
    }

    gInitialised = (result == BME280_RESULT_OK);
    if (!gInitialised) {
        LOG(EVENT_BME280_ERROR, result);
    }

    return result;
}

// Shut-down the humidity/temperature/pressure sensor BME280.
void bme280Deinit()
{
    // TODO
    gInitialised = false;
}

// Get the humidity from the BME280.
Bme280Result getHumidity(unsigned char *pPercentage)
{
    Bme280Result result = BME280_RESULT_ERROR_NOT_INITIALISED;
    uint32_t humidityRaw;
    int32_t vX1;
    char data[4];

    if (gInitialised) {
        data[0] = 0xfd; // hum_msb (2 bytes)
        if (i2cSendReceive(gI2cAddress, data, 1, &(data[1]), 2) == 2) {

            humidityRaw = (data[1] << 8) | data[2];

            vX1 = gTFine - 76800;
            vX1 =  (((((humidityRaw << 14) -(((int32_t) gDigH4) << 20) - (((int32_t) gDigH5) * vX1)) +
                       ((int32_t) 16384)) >> 15) * (((((((vX1 * (int32_t) gDigH6) >> 10) *
                                                    (((vX1 * ((int32_t) gDigH3)) >> 11) + 32768)) >> 10) + 2097152) *
                                                    (int32_t) gDigH2 + 8192) >> 14));
            vX1 = (vX1 - (((((vX1 >> 15) * (vX1 >> 15)) >> 7) * (int32_t) gDigH1) >> 4));
            vX1 = (vX1 < 0 ? 0 : vX1);
            vX1 = (vX1 > 419430400 ? 419430400 : vX1);
            if (pPercentage != NULL) {
                *pPercentage = (unsigned char) ((vX1 >> 12) / 1024);
            }

            result = BME280_RESULT_OK;

        } else {
            result = BME280_RESULT_ERROR_I2C_WRITE_READ;
        }
    }

    if (result != BME280_RESULT_OK) {
        LOG(EVENT_BME280_ERROR, result);
    }

    return result;
}

// Get the pressure from the BME280.
Bme280Result getPressure(unsigned int *pPascalX100)
{
    Bme280Result result = BME280_RESULT_ERROR_NOT_INITIALISED;
    uint32_t pressureRaw;
    int32_t var1;
    int32_t var2;
    uint32_t pressure;

    char data[4];

    if (gInitialised) {
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

                var1 = ((int32_t) gDigP9 * ((int32_t) (((pressure >> 3) * (pressure >> 3)) >> 13))) >> 12;
                var2 = (((int32_t) (pressure >> 2)) * (int32_t) gDigP8) >> 13;
                pressure = (pressure + ((var1 + var2 + gDigP7) >> 4));

                if (pPascalX100 != NULL) {
                    // temperature is hecto-Pascals
                    *pPascalX100 = (unsigned int) pressure;
                }

                result = BME280_RESULT_OK;

            } else {
                result = BME280_RESULT_ERROR_PRESSURE_CALCULATION;
            }
        } else {
            result = BME280_RESULT_ERROR_I2C_WRITE_READ;
        }
    }

    if (result != BME280_RESULT_OK) {
        LOG(EVENT_BME280_ERROR, result);
    }

    return result;
}

// Get the temperature from the BME280.
Bme280Result getTemperature(signed int *pTemperatureCX100)
{
    Bme280Result result = BME280_RESULT_ERROR_NOT_INITIALISED;
    uint32_t temperatureRaw;
    int32_t temperature;
    char data[4];

    if (gInitialised) {
        data[0] = 0xfa; // temp_msb (3 bytes)
        if (i2cSendReceive(gI2cAddress, data, 1, &(data[1]), 3) == 3) {

            temperatureRaw = (data[1] << 12) | (data[2] << 4) | (data[3] >> 4);

            temperature =
            (((((temperatureRaw >> 3) - (gDigT1 << 1))) * gDigT2) >> 11) +
            ((((((temperatureRaw >> 4) - gDigT1) * ((temperatureRaw >> 4) - gDigT1)) >> 12) * gDigT3) >> 14);

            gTFine = temperature;

            temperature = (temperature * 5 + 128) >> 8;

            if (pTemperatureCX100 != NULL) {
                // temperature is in 100ths of a degree
                *pTemperatureCX100 = (signed int) temperature;
            }

            result = BME280_RESULT_OK;

        } else {
            result = BME280_RESULT_ERROR_I2C_WRITE_READ;
        }
    }

    if (result != BME280_RESULT_OK) {
        LOG(EVENT_BME280_ERROR, result);
    }

    return result;
}

// End of file
