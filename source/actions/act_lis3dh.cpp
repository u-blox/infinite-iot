/* The code here is borrowed from:
 *
 * https://os.mbed.com/users/kenjiArai/code/LIS3DH/#0999d25ed7bc
 *
 * All rights remain with the original author(s).
 */

#include <mbed.h>
#include <eh_debug.h>
#include <eh_utilities.h> // for MTX_LOCK()/MTX_UNLOCK()
#include <eh_i2c.h>
#include <act_orientation.h>
#include <act_lis3dh.h>

/**************************************************************************
 * MANIFEST CONSTANTS
 *************************************************************************/

/**************************************************************************
 * LOCAL VARIABLES
 *************************************************************************/

/** Flag so that we know if we've been initialised.
 */
static bool gInitialised = false;

/** The I2C address of the LIS3DH.
 */
static char gI2cAddress = 0;

/** Mutex to protect the against multiple accessors.
 */
static Mutex gMtx;

/** The interrupt threshold LSB value (in milli-G) for a given full-scaled value
 */
static const unsigned int fsToInterruptThresholdLsb[] = {16, 32, 62, 186};

/**************************************************************************
 * STATIC FUNCTIONS
 *************************************************************************/


// Set the interrupt threshold for a pin.
ActionDriver _setInterruptThreshold(unsigned char interrupt,
                                    unsigned int thresholdMG)
{
    ActionDriver result = ACTION_DRIVER_ERROR_I2C_WRITE_READ;
    unsigned char lsbMG;
    unsigned char threshold;
    char data[2];

    // First, read the full scale value
    data[0] = 0x23; // CTRL_REG4
    if (i2cSendReceive(gI2cAddress, data, 1, &data[1], 1) == 1) {
        // Work out what the LSB is, in milli-G, on this basis,
        // for the 7-bit interrupt threshold register
        // Sensitivity is in bits 4 & 5 of data[1]
        lsbMG = fsToInterruptThresholdLsb[(data[1] >> 4) & 0x03];
        // Work out what the threshold value should be
        threshold = thresholdMG / lsbMG;
        if (threshold > 0x7f) {
            threshold = 0x7f;
        }
        // Determine the interrupt threshold register address;
        result = ACTION_DRIVER_ERROR_PARAMETER;
        switch (interrupt) {
            case 1:
                data[0] = 0x32; // INT1_THS
                result = ACTION_DRIVER_OK;
            break;
            case 2:
                data[0] = 0x36; // INT1_THS
                result = ACTION_DRIVER_OK;
            break;
            default:
            break;
        }
        if (result == ACTION_DRIVER_OK) {
            result = ACTION_DRIVER_ERROR_I2C_WRITE;
            // Set the threshold value
            data[1] = (char) threshold;
            if (i2cSendReceive(gI2cAddress, data, 2, NULL, 0) == 0) {
                // Set the duration value
                data[0]++; // INTx_DURATION immediately follows INTx_THS
                data[1] = 1; // The same as the refresh rate
                if (i2cSendReceive(gI2cAddress, data, 2, NULL, 0) == 0) {
                    result = ACTION_DRIVER_OK;
                }
            }
        }
    }

    return result;
}

// Get the interrupt threshold for an interrupt pin.
ActionDriver _getInterruptThreshold(unsigned char interrupt,
                                    unsigned int *pThresholdMG)
{
    ActionDriver result = ACTION_DRIVER_ERROR_NOT_INITIALISED;
    unsigned char lsbMG;
    unsigned char threshold;
    char data[2];

    // Determine the interrupt threshold register address;
    result = ACTION_DRIVER_ERROR_PARAMETER;
    switch (interrupt) {
        case 1:
            data[0] = 0x32; // INT1_THS
            result = ACTION_DRIVER_OK;
        break;
        case 2:
            data[0] = 0x36; // INT2_THS
            result = ACTION_DRIVER_OK;
        break;
        default:
        break;
    }
    if (result == ACTION_DRIVER_OK) {
        result = ACTION_DRIVER_ERROR_I2C_WRITE_READ;
        // Read the register value
        if (i2cSendReceive(gI2cAddress, data, 1, &data[1], 1) == 1) {
            threshold = (unsigned char) data[1];
            // Read the full scale value
            data[0] = 0x23; // CTRL_REG4
            if (i2cSendReceive(gI2cAddress, data, 1, &data[1], 1) == 1) {
                // Work out what the LSB is, in milli-G, on this basis,
                // Sensitivity is in bits 4 & 5 of data[1]
                lsbMG = fsToInterruptThresholdLsb[(data[1] >> 4) & 0x03];
                if (pThresholdMG != NULL) {
                    *pThresholdMG = threshold * lsbMG;
                }
                result = ACTION_DRIVER_OK;
            }
        }
    }

    return result;
}

/**************************************************************************
 * PUBLIC FUNCTIONS
 *************************************************************************/

// Initialise LIS3DH orientation sensor.
ActionDriver lis3dhInit(char i2cAddress)
{
    ActionDriver result;
    char data[2];

    MTX_LOCK(gMtx);

    result = ACTION_DRIVER_OK;

    if (!gInitialised) {
        gI2cAddress = i2cAddress;

        // Read the I_AM_LIS3DH register
        data[0] = 0x0f;
        if (i2cSendReceive(gI2cAddress, data, 1, &(data[1]), 1) == 1) {
            // Should be 0x33
            if (data[1] == 0x33) {
                // Set low power mode
                data[0] = 0x20; // CTRL_REG1
                data[1] = 0x1f; // Low power mode, 1 Hz data rate, all axes
                if (i2cSendReceive(gI2cAddress, data, 2, NULL, 0) == 0) {
                    gInitialised = true;
                } else {
                    result = ACTION_DRIVER_ERROR_I2C_WRITE;
                }
            } else {
                result = ACTION_DRIVER_ERROR_DEVICE_NOT_PRESENT;
            }
        } else {
            result = ACTION_DRIVER_ERROR_I2C_WRITE_READ;
        }
    }

    MTX_UNLOCK(gMtx);

    return result;
}

// Shut-down the LIS3DH orientation sensor.
void lis3dhDeinit()
{
    char data[2];

    MTX_LOCK(gMtx);

    if (gInitialised) {
        // Set power-down mode
        data[0] = 0x20; // CTRL_REG1
        data[1] = 0x0f; // Power down mode
        i2cSendReceive(gI2cAddress, data, 2, NULL, 0);

        gInitialised = false;
    }

    MTX_UNLOCK(gMtx);
}

// Get the orientation.
ActionDriver getOrientation(int *pX, int *pY, int *pZ)
{
    ActionDriver result;
    char data[7];

    MTX_LOCK(gMtx);

    result = ACTION_DRIVER_ERROR_NOT_INITIALISED;

    if (gInitialised) {
        result = ACTION_DRIVER_ERROR_I2C_WRITE_READ;
        data[0] = 0x28 | 0x80; // Start of data registers but with MSB set
                               // in order to perform multi-byte read
        if (i2cSendReceive(gI2cAddress, data, 1, &data[1], 6) == 6) {
            if (pX != NULL) {
                *pX = ((((int) data[2]) << 8) | data[1]) >> 4;
            }
            if (pY != NULL) {
                *pY = ((((int) data[4]) << 8) | data[3]) >> 4;
            }
            if (pZ != NULL) {
                *pZ = ((((int) data[6]) << 8) | data[5]) >> 4;
            }
            result = ACTION_DRIVER_OK;
        }
    }

    MTX_UNLOCK(gMtx);

    return result;
}

// Set the sensitivity of the device.
ActionDriver setSensitivity(unsigned char sensitivity)
{
    ActionDriver result;
    unsigned int thresholdMG1;
    unsigned int thresholdMG2;
    char data[2];

    MTX_LOCK(gMtx);

    result = ACTION_DRIVER_ERROR_NOT_INITIALISED;

    if (gInitialised) {
        if (sensitivity < 4) {
            // If we've change the sensivity then
            // the settings in the interrupt registers
            // need to be updated so read them out first
            result = _getInterruptThreshold(1, &thresholdMG1);
            if (result == ACTION_DRIVER_OK) {
                result = getInterruptThreshold(2, &thresholdMG2);
                if (result == ACTION_DRIVER_OK) {
                    result = ACTION_DRIVER_ERROR_I2C_WRITE_READ;
                    // Now set the sensitivity
                    data[0] = 0x23; // CTRL_REG4
                    // Sensitivity is in bits 4 & 5
                    if (i2cSendReceive(gI2cAddress, data, 1, &data[1], 1) == 1) {
                        data[1] = (data[1] & 0xcf) | (sensitivity << 4);
                        if (i2cSendReceive(gI2cAddress, data, 2, NULL, 0) == 0) {
                            // Put the interrupt sensitivity values back again
                            result = _setInterruptThreshold(1, thresholdMG1);
                            if (result == ACTION_DRIVER_OK) {
                                result = _setInterruptThreshold(2, thresholdMG2);
                            }
                        }
                    }
                }
            }

        } else {
            result = ACTION_DRIVER_ERROR_PARAMETER;
        }
    }

    MTX_UNLOCK(gMtx);

    return result;
}

// Get the sensitivity of the device.
ActionDriver getSensitivity(unsigned char *pSensitivity)
{
    ActionDriver result;
    char data[2];

    MTX_LOCK(gMtx);

    result = ACTION_DRIVER_ERROR_NOT_INITIALISED;

    if (gInitialised) {
        result = ACTION_DRIVER_ERROR_I2C_WRITE_READ;
        data[0] = 0x23; // CTRL_REG4
        // Sensitivity is in bits 4 & 5
        if (i2cSendReceive(gI2cAddress, data, 1, &data[1], 1) == 1) {
            if (pSensitivity != NULL) {
                *pSensitivity = (data[1] >> 4) & 0x03;
            }
            result = ACTION_DRIVER_OK;
        }
    }

    MTX_UNLOCK(gMtx);

    return result;
}

// Set the interrupt threshold for a pin.
ActionDriver setInterruptThreshold(unsigned char interrupt,
                                   unsigned int thresholdMG)
{
    ActionDriver result;

    MTX_LOCK(gMtx);

    result = ACTION_DRIVER_ERROR_NOT_INITIALISED;

    if (gInitialised) {
        result = _setInterruptThreshold(interrupt, thresholdMG);
    }

    MTX_UNLOCK(gMtx);

    return result;
}

// Get the interrupt threshold for an interrupt pin.
ActionDriver getInterruptThreshold(unsigned char interrupt,
                                   unsigned int *pThresholdMG)
{
    ActionDriver result;

    MTX_LOCK(gMtx);

    result = ACTION_DRIVER_ERROR_NOT_INITIALISED;

    if (gInitialised) {
        result = _getInterruptThreshold(interrupt, pThresholdMG);
    }

    MTX_UNLOCK(gMtx);

    return result;
}

// Enable or disable the given interrupt.
ActionDriver setInterruptEnable(unsigned char interrupt,
                                bool enableNotDisable)
{
    ActionDriver result;
    char data[2];

    MTX_LOCK(gMtx);

    result = ACTION_DRIVER_ERROR_NOT_INITIALISED;

    if (gInitialised) {
        // Determine the CFG interrupt register address;
        result = ACTION_DRIVER_ERROR_PARAMETER;
        switch (interrupt) {
            case 1:
                data[0] = 0x30; // INT1_CFG
                result = ACTION_DRIVER_OK;
            break;
            case 2:
                data[0] = 0x34; // INT2_CFG
                result = ACTION_DRIVER_OK;
            break;
            default:
            break;
        }
        if (result == ACTION_DRIVER_OK) {
            // Set the CFG register
            data[1] = 0; // Disabled
            if (enableNotDisable) {
                // Set the xHIE and xLIE bits and OR them
                data[1] = 0x3F;
            }
            result = ACTION_DRIVER_ERROR_I2C_WRITE;
            if (i2cSendReceive(gI2cAddress, data, 2, NULL, 0) == 0) {
                result = ACTION_DRIVER_OK;
            }
        }
    }

    MTX_UNLOCK(gMtx);

    return result;
}

// Get the state of the given interrupt.
ActionDriver getInterruptEnable(unsigned char interrupt,
                                bool *pEnableNotDisable)
{
    ActionDriver result;
    char data[2];

    MTX_LOCK(gMtx);

    result = ACTION_DRIVER_ERROR_NOT_INITIALISED;

    if (gInitialised) {
        // Determine the CFG interrupt register address;
        result = ACTION_DRIVER_ERROR_PARAMETER;
        switch (interrupt) {
            case 1:
                data[0] = 0x30; // INT1_CFG
                result = ACTION_DRIVER_OK;
            break;
            case 2:
                data[0] = 0x34; // INT2_CFG
                result = ACTION_DRIVER_OK;
            break;
            default:
            break;
        }
        if (result == ACTION_DRIVER_OK) {
            result = ACTION_DRIVER_ERROR_I2C_WRITE_READ;
            if (i2cSendReceive(gI2cAddress, data, 1, &(data[1]), 1) == 1) {
                if (pEnableNotDisable != NULL) {
                    *pEnableNotDisable = false;
                    // Any of the xHIE and xLIE bits being set constitutes
                    // enabled
                    if ((data[1] & 0x3F) != 0) {
                        *pEnableNotDisable = true;
                    }
                }
                result = ACTION_DRIVER_OK;
            }
        }
    }

    MTX_UNLOCK(gMtx);

    return result;
}

// End of file
