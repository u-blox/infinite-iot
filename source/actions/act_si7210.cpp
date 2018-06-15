/* The code here is borrowed from:
 *
 * https://os.mbed.com/users/stevew817/code/Si7210/#9bce98da584b
 *
 * All rights remain with the original author(s).
 */

#include <mbed.h>
#include <eh_utilities.h> // for ARRAY_SIZE
#include <eh_debug.h>
#include <eh_i2c.h>
#include <act_magnetic.h>
#include <act_si7210.h>

/**************************************************************************
 * MANIFEST CONSTANTS
 *************************************************************************/

/**************************************************************************
 * LOCAL VARIABLES
 *************************************************************************/

/** Flag so that we know if we've been initialised.
 */
static bool gInitialised = false;

/** The I2C address of the SI7210.
 */
static char gI2cAddress = 0;

/** The raw measurement.
 */
static int gRawFieldStrength;

/** The field strength measurement range.
 */
static FieldStrengthRange gRange;

/** Array to get to the Ax registers (which aren't contiguous).
 */
static const char aXRegisters[] = {0xca, 0xcb, 0xcc, 0xce, 0xcf, 0xd0};

/**************************************************************************
 * STATIC FUNCTIONS
 *************************************************************************/

// Wake the device up by doing an I2C operation.
ActionDriver wakeUp()
{
    ActionDriver result = ACTION_DRIVER_ERROR_I2C_WRITE;
    char data;

    if (i2cSendReceive(gI2cAddress, &data, 1, NULL, 0) == 0) {
        result = ACTION_DRIVER_OK;
    }

    return result;
}

// Put the device back to sleep, optionally keeping the
// measurement timer on
ActionDriver sleep(bool timerOn)
{
    ActionDriver result = ACTION_DRIVER_ERROR_I2C_WRITE_READ;
    char data[2];

    // Set up the sltimena bit (bit 0) in SI72XX_CTRL3
    data[0] = 0xc9; // SI72XX_CTRL3
    if (i2cSendReceive(gI2cAddress, data, 1, &(data[1]), 1) == 0) {
        data[1] = (data[1] & 0xfe) | (0x01 & timerOn);
        result = ACTION_DRIVER_ERROR_I2C_WRITE;
        if (i2cSendReceive(gI2cAddress, data, 2, NULL, 0) == 0) {
            result = ACTION_DRIVER_OK;
        }
    }

    if (result == ACTION_DRIVER_OK) {
        // Clear the stop bit and set the sleep bit in SI72XX_POWER_CTRL
        data[0] = 0xc4; // SI72XX_POWER_CTRL
        result = ACTION_DRIVER_ERROR_I2C_WRITE_READ;
        if (i2cSendReceive(gI2cAddress, data, 1, &(data[1]), 1) == 0) {
            data[1] = (data[1] & 0xf8) | 0x01;
            result = ACTION_DRIVER_ERROR_I2C_WRITE;
            if (i2cSendReceive(gI2cAddress, data, 2, NULL, 0) == 0) {
                result = ACTION_DRIVER_OK;
            }
        }
    }

    return result;
}

// Copy the 6 temperature compensation parameters from OTP at
//the given address into I2C
ActionDriver copyCompensationParameters(char address)
{
    ActionDriver result = ACTION_DRIVER_ERROR_I2C_WRITE;
    char data[7];
    bool success = true;

    data[0] = 0xe1; // SI72XX_OTP_ADDR
                    // the OTP address to read from must go into data[1]
    data[2] = 0xe3; // SI72XX_OTP_CTRL
    data[3] = 0x02; // Enable read with OTP_READ_EN_MASK
    data[4] = 0xe2; // SI72XX_OTP_DATA
                    // The ax register address will be in data[5]
                    // The data that came back from OTP must be put into data[6]
    for (unsigned int x = 0; success && (x < ARRAY_SIZE(aXRegisters)); x++) {
        data[1] = address + x;
        data[5] = aXRegisters[x];
        // Read from the OTP address into data[6]
        result = ACTION_DRIVER_ERROR_I2C_WRITE_READ;
        success = (i2cSendReceive(gI2cAddress, data, 5, &(data[6]), 1) == 0);
        if (success) {
            // Write to the Ax register address what's in data [6]
            result = ACTION_DRIVER_ERROR_I2C_WRITE;
            success = (i2cSendReceive(gI2cAddress, &(data[5]), 2, NULL, 0) == 0);
        }
    }

    if (success) {
        result = ACTION_DRIVER_OK;
    }

    return result;
}

/**************************************************************************
 * PUBLIC FUNCTIONS
 *************************************************************************/

// Initialise SI7210 hall effect sensor.
// TODO set up interrupt
ActionDriver si7210Init(char i2cAddress)
{
    ActionDriver result = ACTION_DRIVER_OK;
    char data[2];

    if (!gInitialised) {
        gI2cAddress = i2cAddress;

        result = wakeUp();
        if (result == ACTION_DRIVER_OK) {
            result = ACTION_DRIVER_ERROR_I2C_WRITE_READ;
            // Read the HW ID register, expecting chipid 1 and revid 4
            data[0] = 0xc0; // SI72XX_HREVID
            if (i2cSendReceive(gI2cAddress, data, 1, &(data[1]), 1) == 0) {
                if (data[1] == 0x14) {
                    gRawFieldStrength = 0;
                    gRange = RANGE_20_MICRO_TESLAS;

                    result = ACTION_DRIVER_OK;
                    gInitialised = true;
                    sleep(true);
                } else {
                    result = ACTION_DRIVER_ERROR_DEVICE_NOT_PRESENT;
                }
            }
        }
    }

    return result;
}

// Shut-down the SI7210 hall effect sensor.
void si7210Deinit()
{
    if (gInitialised) {
        wakeUp();
        sleep(false);
        gInitialised = false;
    }
}

ActionDriver getFieldStrength(unsigned int *pTeslaX1000)
{
    ActionDriver result = ACTION_DRIVER_ERROR_NOT_INITIALISED;
    char data[2];

    if (gInitialised) {
        result = wakeUp();
        if (result == ACTION_DRIVER_OK) {
            result = ACTION_DRIVER_ERROR_I2C_WRITE_READ;
            // Read SI72XX_DSPSIGM
            data[0] = 0xc1; // SI72XX_DSPSIGM
            if (i2cSendReceive(gI2cAddress, data, 1, &(data[1]), 1) == 0) {
                // If the data is new, clear the old and read the rest in
                if (data[1] & 0x80) {
                    gRawFieldStrength = 0;
                    gRawFieldStrength += (data[1] & 0x7f) * 256;
                    data[0] = 0xC2; // SI72XX_DSPSIGL
                    if (i2cSendReceive(gI2cAddress, data, 1, &(data[1]), 1) == 0) {
                        gRawFieldStrength += data[1];
                        gRawFieldStrength -= 16384;
                        result = ACTION_DRIVER_OK;
                    }
                } else {
                    // Just return the existing data
                    result = ACTION_DRIVER_OK;
                }
            }

            if ((result == ACTION_DRIVER_OK) && (pTeslaX1000 != NULL)) {
                switch (gRange) {
                    case RANGE_20_MICRO_TESLAS:
                        // gRawFieldStrength * 1.25
                        *pTeslaX1000 = (gRawFieldStrength / 4) + gRawFieldStrength;
                        result = ACTION_DRIVER_OK;
                    break;
                    case RANGE_200_MICRO_TESLAS:
                        //gRawFieldStrength * 12.5
                        *pTeslaX1000 = (gRawFieldStrength * 12) + (gRawFieldStrength / 2);
                        result = ACTION_DRIVER_OK;
                    break;
                    default:
                        MBED_ASSERT(false);
                    break;
                }
            }
        }
    }

    return result;
}

// Set the field strength range.
ActionDriver setRange(FieldStrengthRange range)
{
    ActionDriver result = ACTION_DRIVER_ERROR_NOT_INITIALISED;

    if (gInitialised) {
        result = wakeUp();
        if (result == ACTION_DRIVER_OK) {
            result = ACTION_DRIVER_ERROR_I2C_WRITE_READ;
            if (range != gRange) {
                switch (range) {
                    case RANGE_20_MICRO_TESLAS:
                        // For 20 microTesla range the 6 parameters
                        // start at OTP address 0x21
                        result = copyCompensationParameters(0x21);
                    break;
                    case RANGE_200_MICRO_TESLAS:
                        // For 200 microTesla range the 6 parameters
                        // start at OTP address 0x27
                        result = copyCompensationParameters(0x27);
                    break;
                    default:
                        result = ACTION_DRIVER_ERROR_PARAMETER;
                    break;
                }
            }
        }
    }

    if (result == ACTION_DRIVER_OK) {
        gRange = range;
    }

    return result;
}

// End of file
