/* The code here is borrowed from:
 *
 * https://os.mbed.com/users/kenjiArai/code/LIS3DH/#0999d25ed7bc
 *
 * All rights remain with the original author(s).
 */

#include <mbed.h>
#include <eh_debug.h>
#include <eh_config.h> // for PIN_INT_ACCELERATION
#include <eh_utilities.h> // for MTX_LOCK()/MTX_UNLOCK()
#include <eh_i2c.h>
#include <act_acceleration.h>
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

/** The interrupt in for the LIS3DH.
 */
static InterruptIn gInterrupt(PIN_INT_ACCELERATION);

/** Remember the sensitivity range.
 */
static unsigned char gSensitivity = 0;

/** The interrupt threshold LSB value (in milli-g) for a given full-scale
 * value.
 */
static const unsigned int fsToInterruptThresholdLsb[] = {16, 32, 62, 186};

/** The measured acceleration LSB value (in milli-g) for a given full-scale
 * value. To work this out, look at section 4.2.3 of the LIS3DH application
 * note.  There it says that, when running in high resolution mode (so
 * 12-bit resolution, expressed in a signed 16-bit number), a reading of
 * 0x4000 represents an acceleration of 1g when the full-scale is +/- 2g,
 * so the LSB in mg is 4000 (the +/- 2g) divided by 65384 (the 0x4000),
 * which is 0.061 mg.  In our case we have only 8-bit resolution but the
 * number is left-justified and so the outcome is the same
 */
static const int fsToMeasuredLsbUG[] = {61, 122, 244, 488};

/**************************************************************************
 * STATIC FUNCTIONS
 *************************************************************************/

// Dump the key registers for debug purposes (not static to avoid warnings)
void lis3dhRegisterDump()
{
    char data[2];

    data[0] = 0x07; // STATUS_REG_AUX
    if (i2cSendReceive(gI2cAddress, data, 1, &(data[1]), 1) == 1) {
        PRINTF("STATUS_REG_AUX (0x%02x): 0x%02x.\n", data[0], data[1]);
    }
    data[0] = 0x08; // OUT_ADC1_L
    if (i2cSendReceive(gI2cAddress, data, 1, &(data[1]), 1) == 1) {
        PRINTF("OUT_ADC1_L (0x%02x): 0x%02x.\n", data[0], data[1]);
    }
    data[0] = 0x09; // OUT_ADC1_H
    if (i2cSendReceive(gI2cAddress, data, 1, &(data[1]), 1) == 1) {
        PRINTF("OUT_ADC1_H (0x%02x): 0x%02x.\n", data[0], data[1]);
    }
    data[0] = 0x0a; // OUT_ADC2_L
    if (i2cSendReceive(gI2cAddress, data, 1, &(data[1]), 1) == 1) {
        PRINTF("OUT_ADC2_L (0x%02x): 0x%02x.\n", data[0], data[1]);
    }
    data[0] = 0x0b; // OUT_ADC2_H
    if (i2cSendReceive(gI2cAddress, data, 1, &(data[1]), 1) == 1) {
        PRINTF("OUT_ADC2_H (0x%02x): 0x%02x.\n", data[0], data[1]);
    }
    data[0] = 0x0c; // OUT_ADC3_L
    if (i2cSendReceive(gI2cAddress, data, 1, &(data[1]), 1) == 1) {
        PRINTF("OUT_ADC3_L (0x%02x): 0x%02x.\n", data[0], data[1]);
    }
    data[0] = 0x0d; // OUT_ADC3_H
    if (i2cSendReceive(gI2cAddress, data, 1, &(data[1]), 1) == 1) {
        PRINTF("OUT_ADC3_H (0x%02x): 0x%02x.\n", data[0], data[1]);
    }
    data[0] = 0x0f; // WHO_AM_I
    if (i2cSendReceive(gI2cAddress, data, 1, &(data[1]), 1) == 1) {
        PRINTF("WHO_AM_I (0x%02x): 0x%02x.\n", data[0], data[1]);
    }
    data[0] = 0x1e; // CTRL_REG0
    if (i2cSendReceive(gI2cAddress, data, 1, &(data[1]), 1) == 1) {
        PRINTF("CTRL_REG0 (0x%02x): 0x%02x.\n", data[0], data[1]);
    }
    data[0] = 0x1f; // TEMP_CFG_REG
    if (i2cSendReceive(gI2cAddress, data, 1, &(data[1]), 1) == 1) {
        PRINTF("TEMP_CFG_REG (0x%02x): 0x%02x.\n", data[0], data[1]);
    }
    data[0] = 0x20; // CTRL_REG1
    if (i2cSendReceive(gI2cAddress, data, 1, &(data[1]), 1) == 1) {
        PRINTF("CTRL_REG1 (0x%02x): 0x%02x.\n", data[0], data[1]);
    }
    data[0] = 0x21; // CTRL_REG2
    if (i2cSendReceive(gI2cAddress, data, 1, &(data[1]), 1) == 1) {
        PRINTF("CTRL_REG2 (0x%02x): 0x%02x.\n", data[0], data[1]);
    }
    data[0] = 0x22; // CTRL_REG3
    if (i2cSendReceive(gI2cAddress, data, 1, &(data[1]), 1) == 1) {
        PRINTF("CTRL_REG3 (0x%02x): 0x%02x.\n", data[0], data[1]);
    }
    data[0] = 0x23; // CTRL_REG4
    if (i2cSendReceive(gI2cAddress, data, 1, &(data[1]), 1) == 1) {
        PRINTF("CTRL_REG4 (0x%02x): 0x%02x.\n", data[0], data[1]);
    }
    data[0] = 0x24; // CTRL_REG5
    if (i2cSendReceive(gI2cAddress, data, 1, &(data[1]), 1) == 1) {
        PRINTF("CTRL_REG5 (0x%02x): 0x%02x.\n", data[0], data[1]);
    }
    data[0] = 0x25; // CTRL_REG6
    if (i2cSendReceive(gI2cAddress, data, 1, &(data[1]), 1) == 1) {
        PRINTF("CTRL_REG6 (0x%02x): 0x%02x.\n", data[0], data[1]);
    }
    data[0] = 0x26; // REFERENCE
    if (i2cSendReceive(gI2cAddress, data, 1, &(data[1]), 1) == 1) {
        PRINTF("REFERENCE (0x%02x): 0x%02x.\n", data[0], data[1]);
    }
    data[0] = 0x27; // STATUS_REG
    if (i2cSendReceive(gI2cAddress, data, 1, &(data[1]), 1) == 1) {
        PRINTF("STATUS_REG (0x%02x): 0x%02x.\n", data[0], data[1]);
    }
    data[0] = 0x28; // OUT_X_L
    if (i2cSendReceive(gI2cAddress, data, 1, &(data[1]), 1) == 1) {
        PRINTF("OUT_X_L (0x%02x): 0x%02x.\n", data[0], data[1]);
    }
    data[0] = 0x29; // OUT_X_H
    if (i2cSendReceive(gI2cAddress, data, 1, &(data[1]), 1) == 1) {
        PRINTF("OUT_X_H (0x%02x): 0x%02x.\n", data[0], data[1]);
    }
    data[0] = 0x2a; // OUT_Y_L
    if (i2cSendReceive(gI2cAddress, data, 1, &(data[1]), 1) == 1) {
        PRINTF("OUT_Y_L (0x%02x): 0x%02x.\n", data[0], data[1]);
    }
    data[0] = 0x2b; // OUT_Y_H
    if (i2cSendReceive(gI2cAddress, data, 1, &(data[1]), 1) == 1) {
        PRINTF("OUT_Y_H (0x%02x): 0x%02x.\n", data[0], data[1]);
    }
    data[0] = 0x2c; // OUT_Z_L
    if (i2cSendReceive(gI2cAddress, data, 1, &(data[1]), 1) == 1) {
        PRINTF("OUT_Z_L (0x%02x): 0x%02x.\n", data[0], data[1]);
    }
    data[0] = 0x2d; // OUT_Z_H
    if (i2cSendReceive(gI2cAddress, data, 1, &(data[1]), 1) == 1) {
        PRINTF("OUT_Z_H (0x%02x): 0x%02x.\n", data[0], data[1]);
    }
    data[0] = 0x2e; // FIFO_CTRL_REG
    if (i2cSendReceive(gI2cAddress, data, 1, &(data[1]), 1) == 1) {
        PRINTF("FIFO_CTRL_REG (0x%02x): 0x%02x.\n", data[0], data[1]);
    }
    data[0] = 0x2f; // FIFO_SRC_REG
    if (i2cSendReceive(gI2cAddress, data, 1, &(data[1]), 1) == 1) {
        PRINTF("FIFO_SRC_REG (0x%02x): 0x%02x.\n", data[0], data[1]);
    }
    data[0] = 0x30; // INT1_CFG
    if (i2cSendReceive(gI2cAddress, data, 1, &(data[1]), 1) == 1) {
        PRINTF("INT1_CFG (0x%02x): 0x%02x.\n", data[0], data[1]);
    }
    data[0] = 0x31; // INT1_SRC
    if (i2cSendReceive(gI2cAddress, data, 1, &(data[1]), 1) == 1) {
        PRINTF("INT1_SRC (0x%02x): 0x%02x.\n", data[0], data[1]);
    }
    data[0] = 0x32; // INT1_THS
    if (i2cSendReceive(gI2cAddress, data, 1, &(data[1]), 1) == 1) {
        PRINTF("INT1_THS (0x%02x): 0x%02x.\n", data[0], data[1]);
    }
    data[0] = 0x33; // INT1_DURATION
    if (i2cSendReceive(gI2cAddress, data, 1, &(data[1]), 1) == 1) {
        PRINTF("INT1_DURATION (0x%02x): 0x%02x.\n", data[0], data[1]);
    }
    data[0] = 0x34; // INT2_CFG
    if (i2cSendReceive(gI2cAddress, data, 1, &(data[1]), 1) == 1) {
        PRINTF("INT2_CFG (0x%02x): 0x%02x.\n", data[0], data[1]);
    }
    data[0] = 0x35; // INT2_SRC
    if (i2cSendReceive(gI2cAddress, data, 1, &(data[1]), 1) == 1) {
        PRINTF("INT2_SRC (0x%02x): 0x%02x.\n", data[0], data[1]);
    }
    data[0] = 0x36; // INT2_THS
    if (i2cSendReceive(gI2cAddress, data, 1, &(data[1]), 1) == 1) {
        PRINTF("INT2_THS (0x%02x): 0x%02x.\n", data[0], data[1]);
    }
    data[0] = 0x37; // INT2_DURATION
    if (i2cSendReceive(gI2cAddress, data, 1, &(data[1]), 1) == 1) {
        PRINTF("INT2_DURATION (0x%02x): 0x%02x.\n", data[0], data[1]);
    }
    data[0] = 0x38; // CLICK_CFG
    if (i2cSendReceive(gI2cAddress, data, 1, &(data[1]), 1) == 1) {
        PRINTF("CLICK_CFG (0x%02x): 0x%02x.\n", data[0], data[1]);
    }
    data[0] = 0x39; // CLICK_SRC
    if (i2cSendReceive(gI2cAddress, data, 1, &(data[1]), 1) == 1) {
        PRINTF("CLICK_SRC (0x%02x): 0x%02x.\n", data[0], data[1]);
    }
    data[0] = 0x3a; // CLICK_THS
    if (i2cSendReceive(gI2cAddress, data, 1, &(data[1]), 1) == 1) {
        PRINTF("CLICK_THS (0x%02x): 0x%02x.\n", data[0], data[1]);
    }
    data[0] = 0x3b; // TIME_LIMIT
    if (i2cSendReceive(gI2cAddress, data, 1, &(data[1]), 1) == 1) {
        PRINTF("TIME_LIMIT (0x%02x): 0x%02x.\n", data[0], data[1]);
    }
    data[0] = 0x3c; // TIME_LATENCY
    if (i2cSendReceive(gI2cAddress, data, 1, &(data[1]), 1) == 1) {
        PRINTF("TIME_LATENCY (0x%02x): 0x%02x.\n", data[0], data[1]);
    }
    data[0] = 0x3d; // TIME_WINDOW
    if (i2cSendReceive(gI2cAddress, data, 1, &(data[1]), 1) == 1) {
        PRINTF("TIME_WINDOW (0x%02x): 0x%02x.\n", data[0], data[1]);
    }
    data[0] = 0x3e; // ACT_THS
    if (i2cSendReceive(gI2cAddress, data, 1, &(data[1]), 1) == 1) {
        PRINTF("ACT_THS (0x%02x): 0x%02x.\n", data[0], data[1]);
    }
    data[0] = 0x3f; // ACT_DUR
    if (i2cSendReceive(gI2cAddress, data, 1, &(data[1]), 1) == 1) {
        PRINTF("ACT_DUR (0x%02x): 0x%02x.\n", data[0], data[1]);
    }
}

// Set the interrupt threshold for a pin.
ActionDriver _setInterruptThreshold(unsigned char interrupt,
                                    unsigned int thresholdMG)
{
    ActionDriver result = ACTION_DRIVER_ERROR_I2C_WRITE_READ;
    unsigned char lsbMG;
    unsigned char threshold;
    char data[2];

    lsbMG = fsToInterruptThresholdLsb[gSensitivity];
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
            result = ACTION_DRIVER_OK;
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
            // Work out what the LSB is, in milli-g, on this basis,
            // Sensitivity is in bits 4 & 5 of data[1]
            lsbMG = fsToInterruptThresholdLsb[gSensitivity];
            if (pThresholdMG != NULL) {
                *pThresholdMG = threshold * lsbMG;
            }
            result = ACTION_DRIVER_OK;
        }
    }

    return result;
}

// Convert a reading into milli-g.
static int readingToMG(char dataHigh)
{
    int data;

    // Convert dataHigh into a signed int
    data = (((int) (dataHigh)) << 24) >> 16;

    // Multiple by the correct scale value
    return (data * fsToMeasuredLsbUG[gSensitivity]) / 1000;
}

/**************************************************************************
 * PUBLIC FUNCTIONS: GENERIC
 *************************************************************************/

// Get the acceleration values.
ActionDriver getAcceleration(int *pXGX100, int *pYGX1000, int *pZGX1000)
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
            // Note that in low power mode the result is only 8 bits
            if (pXGX100 != NULL) {
                *pXGX100 = readingToMG(data[2]);
            }
            if (pYGX1000 != NULL) {
                *pYGX1000 = readingToMG(data[4]);
            }
            if (pZGX1000 != NULL) {
                *pZGX1000 = readingToMG(data[6]);
            }
            result = ACTION_DRIVER_OK;
        }
    }

    MTX_UNLOCK(gMtx);

    return result;
}

/**************************************************************************
 * PUBLIC FUNCTIONS: LIS3DH SPECIFIC
 *************************************************************************/

// Initialise LIS3DH accelerometer.
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

// Shut-down the LIS3DH accelerometer.
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

// Set the sensitivity of the device.
ActionDriver lis3dhSetSensitivity(unsigned char sensitivity)
{
    ActionDriver result;
    unsigned int thresholdMG1;
    unsigned int thresholdMG2;
    char data[2];

    MTX_LOCK(gMtx);

    result = ACTION_DRIVER_ERROR_NOT_INITIALISED;

    if (gInitialised) {
        if (sensitivity < 4) {
            // If we've change the sensitivity then
            // the settings in the interrupt registers
            // need to be updated so read them out first
            result = _getInterruptThreshold(1, &thresholdMG1);
            if (result == ACTION_DRIVER_OK) {
                result = _getInterruptThreshold(2, &thresholdMG2);
                if (result == ACTION_DRIVER_OK) {
                    result = ACTION_DRIVER_ERROR_I2C_WRITE_READ;
                    // Now set the sensitivity
                    data[0] = 0x23; // CTRL_REG4
                    // Sensitivity is in bits 4 & 5
                    if (i2cSendReceive(gI2cAddress, data, 1, &data[1], 1) == 1) {
                        data[1] = (data[1] & 0xcf) | (sensitivity << 4);
                        if (i2cSendReceive(gI2cAddress, data, 2, NULL, 0) == 0) {
                            gSensitivity = sensitivity;
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
ActionDriver lis3dhGetSensitivity(unsigned char *pSensitivity)
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
ActionDriver lis3dhSetInterruptThreshold(unsigned char interrupt,
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
ActionDriver lis3dhGetInterruptThreshold(unsigned char interrupt,
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
ActionDriver lis3dhSetInterruptEnable(unsigned char interrupt,
                                      bool enableNotDisable)
{
    ActionDriver result;
    char data[2];

    MTX_LOCK(gMtx);

    result = ACTION_DRIVER_ERROR_NOT_INITIALISED;

    if (gInitialised) {
        result = ACTION_DRIVER_ERROR_PARAMETER;
        if ((interrupt > 0) && (interrupt <= 2)) {
            // Set the high pass filter on, in auto mode
            // and send filtered data to the output registers
            data[0] = 0x21; // CTRL_REG2
            data[1] = 0xc8;
            data[1] |= 1 << (interrupt - 1); // HP_IA1 or HP_IA2
            if (i2cSendReceive(gI2cAddress, data, 2, NULL, 0) == 0) {
                // Read the REFERENCE register to set the filter up.
                data[0] = 0x26; // REFERENCE
                result = ACTION_DRIVER_ERROR_I2C_WRITE_READ;
                if (i2cSendReceive(gI2cAddress, data, 1, &(data[1]), 1) == 1) {
                    // Set the top-level CFG register to enable the interrupt pin
                    // For interrupt 1 set CTRL_REG3 bit I1_IA1 (0x40)
                    // for interrupt 2 set CTRL_REG6 bit I2_IA1 (0X40)
                    if (interrupt == 1) {
                        data[0] = 0x22; // CTRL_REG3
                    } else {
                        data[0] = 0x25; // CTRL_REG6
                    }
                    if (i2cSendReceive(gI2cAddress, data, 1, &(data[1]), 1) == 1) {
                        data[1] |= 0x40;
                        result = ACTION_DRIVER_ERROR_I2C_WRITE;
                        if (i2cSendReceive(gI2cAddress, data, 2, NULL, 0) == 0) {
                            result = ACTION_DRIVER_OK;
                        }
                    }
                    if (result == ACTION_DRIVER_OK) {
                        // Latch the interrupt in CTRL_REG5
                        // For interrupt 1 set bit LIR_INT1 (0x08)
                        // for interrupt 2 set bit LIR_INT2 (0x02)
                        result = ACTION_DRIVER_ERROR_I2C_WRITE_READ;
                        data[0] = 0x24; // CTRL_REG5
                        if (i2cSendReceive(gI2cAddress, data, 1, &(data[1]), 1) == 1) {
                            if (interrupt == 1) {
                                data[1] |= 0x08;
                            } else {
                                data[1] |= 0x02;
                            }
                            result = ACTION_DRIVER_ERROR_I2C_WRITE;
                            if (i2cSendReceive(gI2cAddress, data, 2, NULL, 0) == 0) {
                                // Now, finally, configure the interrupt
                                switch (interrupt) {
                                    case 1:
                                        data[0] = 0x33; // INT1_DURATION
                                    break;
                                    case 2:
                                        data[0] = 0x37; // INT2_DURATION
                                    break;
                                    default:
                                    break;
                                }
                                // Set the duration value
                                data[1] = 0; // Zero duration since it is being latched
                                if (i2cSendReceive(gI2cAddress, data, 2, NULL, 0) == 0) {
                                    // Set the CFG register, which is 3 back from the duration
                                    // register
                                    data[0] -= 3;
                                    data[1] = 0; // Disabled
                                    if (enableNotDisable) {
                                        // Set the xHIE and OR them
                                        data[1] = 0x2a;
                                    }
                                    if (i2cSendReceive(gI2cAddress, data, 2, NULL, 0) == 0) {
                                        result = ACTION_DRIVER_OK;
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    MTX_UNLOCK(gMtx);

    return result;
}

// Get the state of the given interrupt.
ActionDriver lis3dhGetInterruptEnable(unsigned char interrupt,
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


// Clear the interrupt.
ActionDriver lis3dhClearInterrupt(unsigned char interrupt)
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
                data[0] = 0x31; // INT1_SRC
                result = ACTION_DRIVER_OK;
            break;
            case 2:
                data[0] = 0x35; // INT2_SRC
                result = ACTION_DRIVER_OK;
            break;
            default:
            break;
        }
        if (result == ACTION_DRIVER_OK) {
            result = ACTION_DRIVER_ERROR_I2C_WRITE_READ;
            // Just reading the register is enough to clear the interrupt
            if (i2cSendReceive(gI2cAddress, data, 1, &(data[1]), 1) == 1) {
                result = ACTION_DRIVER_ERROR_NO_INTERRUPT;
                if (data[1] & 0x40) { // IA bit
                    result = ACTION_DRIVER_OK;
                }
            }
        }
    }

    MTX_UNLOCK(gMtx);

    return result;
}

// End of file
