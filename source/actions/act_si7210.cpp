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

// Dump the key registers for debug purposes (not static to avoid warnings)
void registerDump()
{
    char data[2];

    data[0] = 0xc3; // SI72XX_DSPSIGSEL
    if (i2cSendReceive(gI2cAddress, data, 1, &(data[1]), 1) == 1) {
        PRINTF("DSPSigSel (0x%02x): 0x%02x.\n", data[0], data[1]);
    }
    data[0] = 0xc4; // SI72XX_POWER_CTRL
    if (i2cSendReceive(gI2cAddress, data, 1, &(data[1]), 1) == 1) {
        PRINTF("PowerCtrl (0x%02x): 0x%02x.\n", data[0], data[1]);
    }
    data[0] = 0xc5; // SI72XX_ARAUTOINC
    if (i2cSendReceive(gI2cAddress, data, 1, &(data[1]), 1) == 1) {
        PRINTF("ARAutoInc (0x%02x): 0x%02x.\n", data[0], data[1]);
    }
    data[0] = 0xc6; // SI72XX_CTRL1
    if (i2cSendReceive(gI2cAddress, data, 1, &(data[1]), 1) == 1) {
        PRINTF("Ctrl1 (0x%02x): 0x%02x.\n", data[0], data[1]);
    }
    data[0] = 0xc7; // SI72XX_CTRL2
    if (i2cSendReceive(gI2cAddress, data, 1, &(data[1]), 1) == 1) {
        PRINTF("Ctrl2 (0x%02x): 0x%02x.\n", data[0], data[1]);
    }
    data[0] = 0xc8; // SI72XX_SLTIME
    if (i2cSendReceive(gI2cAddress, data, 1, &(data[1]), 1) == 1) {
        PRINTF("SlTime (0x%02x): 0x%02x.\n", data[0], data[1]);
    }
    data[0] = 0xc9; // SI72XX_CTRL3
    if (i2cSendReceive(gI2cAddress, data, 1, &(data[1]), 1) == 1) {
        PRINTF("Ctrl3 (0x%02x): 0x%02x.\n", data[0], data[1]);
    }
    data[0] = 0xcd; // SI72XX_CTRL4
    if (i2cSendReceive(gI2cAddress, data, 1, &(data[1]), 1) == 1) {
        PRINTF("Ctrl4 (0x%02x): 0x%02x.\n", data[0], data[1]);
    }
    data[0] = 0xca; // SI72XX_A0
    if (i2cSendReceive(gI2cAddress, data, 1, &(data[1]), 1) == 1) {
        PRINTF("A0 (0x%02x): 0x%02x.\n", data[0], data[1]);
    }
    data[0] = 0xcb; // SI72XX_A1
    if (i2cSendReceive(gI2cAddress, data, 1, &(data[1]), 1) == 1) {
        PRINTF("A1 (0x%02x): 0x%02x.\n", data[0], data[1]);
    }
    data[0] = 0xcc; // SI72XX_A2
    if (i2cSendReceive(gI2cAddress, data, 1, &(data[1]), 1) == 1) {
        PRINTF("A2 (0x%02x): 0x%02x.\n", data[0], data[1]);
    }
    data[0] = 0xce; // SI72XX_A3
    if (i2cSendReceive(gI2cAddress, data, 1, &(data[1]), 1) == 1) {
        PRINTF("A3 (0x%02x): 0x%02x.\n", data[0], data[1]);
    }
    data[0] = 0xcf; // SI72XX_A4
    if (i2cSendReceive(gI2cAddress, data, 1, &(data[1]), 1) == 1) {
        PRINTF("A4 (0x%02x): 0x%02x.\n", data[0], data[1]);
    }
    data[0] = 0xd0; // SI72XX_A5
    if (i2cSendReceive(gI2cAddress, data, 1, &(data[1]), 1) == 1) {
        PRINTF("A5 (0x%02x): 0x%02x.\n", data[0], data[1]);
    }
}

// Wake the device up by doing an I2C read operation.
// The device is returned to idle with the stop bit set.
ActionDriver wakeUp()
{
    ActionDriver result = ACTION_DRIVER_ERROR_I2C_WRITE;
    char data;

    if (i2cSendReceive(gI2cAddress, NULL, 0, &data, 1) == 1) {
        // Don't necessarily need to do this since only a 10 uS delay
        // is required but I feel safer making sure.
        wait_ms(1);
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

    // To put the part to sleep, clear the stop bit (bit 1 in
    // SI72XX_POWER_CTRL), and if timed measurements are NOT required,
    // then also set the sleep bit (bit 0 in SI72XX_POWER_CTRL).
    // There should be no need to fiddle with the sltimena bit
    // (bit 0 in SI72XX_CTRL3) as it defaults to 1.
    data[0] = 0xc4; // SI72XX_POWER_CTRL
    if (i2cSendReceive(gI2cAddress, data, 1, &(data[1]), 1) == 1) {
        data[1] &= 0xfd; // stop bit
        if (!timerOn) {
            data[1] |= 0x01; // sleep bit
        }
        result = ACTION_DRIVER_ERROR_I2C_WRITE;
        if (i2cSendReceive(gI2cAddress, data, 2, NULL, 0) == 0) {
            result = ACTION_DRIVER_OK;
        }
    }

    return result;
}

// Copy the 6 temperature compensation parameters from OTP at
//the given address into I2C
ActionDriver copyCompensationParameters(char address)
{
    ActionDriver result = ACTION_DRIVER_ERROR_I2C_WRITE;
    Timer timer;
    char data[7];
    bool keepGoing = true;
    bool success = false;

    // Note: a line the contents of data[] up here so that the
    // writes could be done in one sequence, however the chip
    // doesn't work like that, each register write must be done
    // as a discrete operation
    data[0] = 0xe1; // SI72XX_OTP_ADDR
                    // the OTP address to read from must go into data[1]
    data[2] = 0xe3; // SI72XX_OTP_CTRL
    data[3] = 0x02; // Enable read with OTP_READ_EN_MASK
    data[4] = 0xe2; // SI72XX_OTP_DATA
                    // The ax register address will be in data[5]
                    // The data that came back from OTP must be put into data[6]
    for (unsigned int x = 0; keepGoing && (x < ARRAY_SIZE(aXRegisters)); x++) {
        data[1] = address + x;
        data[5] = aXRegisters[x];
        // Ask for the data by loading the OTP address and enabling read
        result = ACTION_DRIVER_ERROR_I2C_WRITE;
        if (i2cSendReceive(gI2cAddress, &(data[0]), 2, NULL, 0) == 0) {
            if (i2cSendReceive(gI2cAddress, &(data[2]), 2, NULL, 0) == 0) {
                timer.reset();
                timer.start();
                // Wait for the otp_busy bit to be 0, using data[6] as temporary storage
                success = false;
                while (!success && (timer.read_ms() < SI7210_WAIT_FOR_OTP_DATA_MS)) {
                    result = ACTION_DRIVER_ERROR_I2C_WRITE_READ;
                    if (i2cSendReceive(gI2cAddress, &(data[2]), 1, &(data[6]), 1) == 1) {
                        success = ((data[6] & 0x01) == 0);
                        wait_ms(10); // Relax a little
                    }
                }
                timer.stop();
                if (success) {
                    // Read from the OTP address into data[6]
                    result = ACTION_DRIVER_ERROR_I2C_WRITE_READ;
                    success = (i2cSendReceive(gI2cAddress, &(data[4]), 1, &(data[6]), 1) == 1);
                    if (success) {
                        // Write to the Ax register address what's in data [6]
                        result = ACTION_DRIVER_ERROR_I2C_WRITE;
                        success = (i2cSendReceive(gI2cAddress, &(data[5]), 2, NULL, 0) == 0);
                    }
                }
            }
        }
        keepGoing = success;
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
    bool success = false;
    Timer timer;
    char data[2];

    if (!gInitialised) {
        gI2cAddress = i2cAddress;

        result = wakeUp();
        if (result == ACTION_DRIVER_OK) {
            result = ACTION_DRIVER_ERROR_I2C_WRITE_READ;
            // Read the HW ID register, expecting chipid 1 and revid 4
            data[0] = 0xc0; // SI72XX_HREVID
            if (i2cSendReceive(gI2cAddress, data, 1, &(data[1]), 1) == 1) {
                if (data[1] == 0x14) {
                    // Set the range to default with the correct compensation
                    // parameters
                    gRange = RANGE_20_MICRO_TESLAS;
                    result = copyCompensationParameters(0x21);
                    if (result == ACTION_DRIVER_OK) {
                        // Force one measurement to be taken
                        data[0] = 0xc4; // SI72XX_POWER_CTRL
                        if (i2cSendReceive(gI2cAddress, data, 1, &(data[1]), 1) == 1) {
                            data[1] = (data[1] | 0x04) & 0xfd; // set one-burst bit and clear stop bit
                            result = ACTION_DRIVER_ERROR_I2C_WRITE;
                            if (i2cSendReceive(gI2cAddress, data, 2, NULL, 0) == 0) {
                                result = ACTION_DRIVER_ERROR_TIMEOUT;
                                timer.reset();
                                timer.start();
                                data[0] = 0xc1; // SI72XX_DSPSIGM
                                while (!success && (timer.read_ms() < SI7210_WAIT_FOR_FIRST_MEASUREMENT_MS)) {
                                    if (i2cSendReceive(gI2cAddress, data, 1, &(data[1]), 1) == 1) {
                                        success = ((data[1] & 0x80) != 0);  // Top bit indicates a new measurement
                                    }
                                    wait_ms(100); // Relax a little
                                }
                                timer.stop();
                                if (success) {
                                    // Return to sleep with the measurement timer running
                                    result = sleep(true);
                                    if (result == ACTION_DRIVER_OK) {
                                        gInitialised = true;
                                    }
                                }
                            }
                        }
                    }
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
            if (i2cSendReceive(gI2cAddress, data, 1, &(data[1]), 1) == 1) {
                // If the data is new, clear the old and read the other half in
                if ((data[1] & 0x80) != 0) {
                    gRawFieldStrength = 0;
                    gRawFieldStrength += (((unsigned char) data[1]) & 0x7f) << 8;
                    data[0] = 0xC2; // SI72XX_DSPSIGL
                    if (i2cSendReceive(gI2cAddress, data, 1, &(data[1]), 1) == 1) {
                        gRawFieldStrength += (unsigned char) data[1];
                        gRawFieldStrength -= 0x4000;
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

            // Return to sleep with the measurement timer running
            sleep(true);
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

            // Return to sleep with the measurement timer running
            sleep(true);
        }
    }

    if (result == ACTION_DRIVER_OK) {
        gRange = range;
    }

    return result;
}

// Get the current measurement range.
FieldStrengthRange getRange()
{
    return gRange;
}

// End of file
