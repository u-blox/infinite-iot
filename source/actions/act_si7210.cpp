/* The code here is borrowed from:
 *
 * https://os.mbed.com/users/stevew817/code/Si7210/#9bce98da584b
 *
 * All rights remain with the original author(s).
 */

#include <mbed.h>
#include <eh_utilities.h> // for ARRAY_SIZE and MTX_LOCK()/MTX_UNLOCK()
#include <eh_debug.h>
#include <eh_config.h> // for PIN_INT_MAGNETIC
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

/** Mutex to protect the against multiple accessors.
 */
static Mutex gMtx;

/** The interrupt in for the SI7210.
 */
static InterruptIn gInterrupt(PIN_INT_MAGNETIC);

/** Flag to indicate the interrupt has gone off.
 */
static bool gTwasMe;

/** Event queue to use in interrupt callback.
 */
static EventQueue *gpEventQueue;

/** Event callback to be called on an interrupt
 */
static void (*gpEventCallback) (EventQueue *);

/** The raw measurement.
 */
static int gRawFieldStrength;

/** The field strength measurement range.
 */
static Si7210FieldStrengthRange gRange;

/** Array to get to the Ax registers (which aren't contiguous).
 */
static const char aXRegisters[] = {0xca, 0xcb, 0xcc, 0xce, 0xcf, 0xd0};

/**************************************************************************
 * STATIC FUNCTIONS
 *************************************************************************/

// Dump the key registers for debug purposes (not static to avoid warnings)
void si7210RegisterDump()
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

// Dump the OTP registers (for debug purposes).
void si7210OtpRegisterDump()
{
    Timer timer;
    char data[6];
    bool keepGoing = true;
    bool success = false;

    data[0] = 0xe1; // SI72XX_OTP_ADDR
                    // The OTP address to read from goes in data[1]
    data[2] = 0xe3; // SI72XX_OTP_CTRL
    data[3] = 0x02; // Enable read with OTP_READ_EN_MASK
    data[4] = 0xe2; // SI72XX_OTP_DATA
    for (unsigned int x = 0; keepGoing && (x < 0x41); x++) {
        data[1] = 0x04 + x;
        // Ask for the data by loading the OTP address and enabling read
        if (i2cSendReceive(gI2cAddress, &(data[0]), 2, NULL, 0) == 0) {
            if (i2cSendReceive(gI2cAddress, &(data[2]), 2, NULL, 0) == 0) {
                timer.reset();
                timer.start();
                // Wait for the otp_busy bit to be 0, using data[5] as temporary storage
                success = false;
                while (!success && (timer.read_ms() < SI7210_WAIT_FOR_OTP_DATA_MS)) {
                    if (i2cSendReceive(gI2cAddress, &(data[2]), 1, &(data[5]), 1) == 1) {
                        success = ((data[5] & 0x01) == 0);
                        Thread::wait(10); // Relax a little
                    }
                }
                timer.stop();
                if (success) {
                    // Read from the OTP address into data[5]
                    success = (i2cSendReceive(gI2cAddress, &(data[4]), 1, &(data[5]), 1) == 1);
                    if (success) {
                        PRINTF("OTP 0x%02x: 0x%02x.\n", data[1], data[5]);
                    }
                }
            }
        }
        keepGoing = success;
    }
}

// Interrupt callback
static void interruptCallback()
{
    if (!gTwasMe) {
        gTwasMe = true;
        if ((gpEventQueue != NULL) && (gpEventCallback != NULL)) {
            gpEventQueue->call(callback(gpEventCallback, gpEventQueue));
        }
    }
}

// Wake the device up by doing an I2C read operation.
// The device will be returned to idle with the stop bit set.
static ActionDriver wakeUp()
{
    ActionDriver result = ACTION_DRIVER_ERROR_I2C_WRITE;
    char data;

    if (i2cSendReceive(gI2cAddress, NULL, 0, &data, 1) == 1) {
        // Don't necessarily need to do this since only a 10 uS delay
        // is required but I feel safer making sure.
        Thread::wait(1);
        result = ACTION_DRIVER_OK;
    }

    return result;
}

// Put the device back to sleep, optionally keeping the
// measurement timer on
static ActionDriver sleep(bool timerOn)
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
// the given address into I2C
static ActionDriver copyCompensationParameters(char address)
{
    ActionDriver result = ACTION_DRIVER_ERROR_I2C_WRITE;
    Timer timer;
    char data[7];
    bool keepGoing = true;
    bool success = false;

    // Note: I lined the contents of data[] up here so that the
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
                        Thread::wait(10); // Relax a little
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

// Set up the interrupt.
// See section 4.1.3 of the si7210 data sheet.
static ActionDriver _setInterrupt(unsigned int threshold,
                                  unsigned int hysteresis,
                                  bool activeHigh)
{
    ActionDriver result = ACTION_DRIVER_ERROR_I2C_WRITE;
    int x;
    int y;
    char data[2];

    data[0] = 0xc6; // SI72XX_CTRL1
    data[1] = 0;
    // Sort the sw_low4field bit
    if (!activeHigh) {
        data[1] = 0x80;
    }
    // Sort out the threshold
    // Account for the range
    threshold /= 5;
    if (gRange == SI7210_RANGE_200_MILLI_TESLAS) {
        threshold /= 10;
    }
    // The maximum threshold number that can be
    // coded into sw_op is 3840 and the minimum 16
    // except for the special value of 0.
    if (threshold != 0) {
        if (threshold > 3840) {
            threshold = 3840;
        } else if (threshold < 16) {
            threshold = 16;
        }
    }
    // Now code the value
    if (threshold == 0) {
        data[1] |= 0x7f;
    } else {
        x = 0;
        y = (int) threshold;
        // Shift it down until threshold - 16 is
        // less than 0xF (4 bits)
        while ((y - 16) > 0xF) {
            y >>= 1;
            x++;
        }
        data[1] |= (y - 16) | (x << 4);
    }
    // Write the sw_op/sw_low4field values
    if (i2cSendReceive(gI2cAddress, data, 2, NULL, 0) == 0) {
        // Now sort out the hysteresis
        data[0] = 0xc7; // SI72XX_CTRL2
        data[1] = 0;
        if (threshold == 0) {
            // In latch mode each bit represents
            // twice what it would otherwise,
            // so divide by 2
            hysteresis >>= 1;
        }
        // If a hysteresis of 0 is requested
        // just use the special value of 0x3F
        if (hysteresis == 0) {
            data[1] = 0x3F;
        } else {
            // Account for the range
            hysteresis /= 5;
            if (gRange == SI7210_RANGE_200_MILLI_TESLAS) {
                hysteresis /= 10;
            }
            // The maximum hysteresis number that can be
            // coded into sw_hyst is 1792 and minimum 8
            if (hysteresis > 1792) {
                hysteresis = 1792;
            } else if (hysteresis < 8) {
                hysteresis = 8;
            }
            // Now code the value
            x = 0;
            y = (int) hysteresis;
            // Shift it down until hysteresis - 8 is
            // less than 0x7 (3 bits)
            while ((y - 8) > 0x7) {
                y >>= 1;
                x++;
            }
            data[1] |= (y - 8) | (x << 3);
        }
        // Write the sw_hyst value (sw_fieldpolsel is left
        // at zero always)
        if (i2cSendReceive(gI2cAddress, data, 2, NULL, 0) == 0) {
            result = ACTION_DRIVER_OK;
        }
    }

    return result;
}


// Get the interrupt.
static ActionDriver _getInterrupt(unsigned int *pThresholdTeslaX1000,
                                  unsigned int *pHysteresisTeslaX1000,
                                  bool *pActiveHigh)
{
    ActionDriver result = ACTION_DRIVER_ERROR_I2C_WRITE_READ;
    unsigned int threshold;
    unsigned int hysteresis;
    bool activeHigh;
    char data[4];

    // Read SI72XX_CTRL1 and SI72XX_CTRL2
    data[0] = 0xc6; // SI72XX_CTRL1
    data[2] = 0xc7; // SI72XX_CTRL2
    if ((i2cSendReceive(gI2cAddress, &(data[0]), 1, &(data[1]), 1) == 1) &&
        (i2cSendReceive(gI2cAddress, &(data[2]), 1, &(data[3]), 1)) == 1) {

        // Threshold is bits 0 to 6 of SI72XX_CTRL1
        threshold = ((int) (16 + (data[1] & 0x0F))) << ((data[1] & 0x70) >> 4);
        // sw_op coding is that 16 to 3840 are valid thresholds and
        // then the special value of 0x7F represents 0
        if (threshold > 3840) {
            threshold = 0;
        }
        // For the 20 milli-Tesla range
        threshold *= 5;
        if (gRange == SI7210_RANGE_200_MILLI_TESLAS) {
            threshold *= 10;
        }
        if (pThresholdTeslaX1000 != NULL) {
            *pThresholdTeslaX1000 = (unsigned int) threshold;
        }

        // activeHigh is bit 7 of SI72XX_CTRL1
        activeHigh = ((data[1] & 0x80) == 0);
        if (pActiveHigh != NULL) {
            *pActiveHigh = activeHigh;
        }

        // Hysteresis is bits 0 to 5 of SI72XX_CTRL2
        hysteresis = ((int) (8 + (data[3] & 0x07))) << ((data[3] & 0x38) >> 3);
        // sw_hyst coding is that 8 to 1792 are valid and
        // the special value of 0x3F, which equates to 1920,
        // represents 0
        if (hysteresis > 1792) {
            hysteresis = 0;
        }
        // If threshold is zero, the value for each bit is doubled
        if (threshold == 0) {
            hysteresis <<= 1;
        }
        // For the 20 milli-Tesla range
        hysteresis *= 5;
        if (gRange == SI7210_RANGE_200_MILLI_TESLAS) {
            hysteresis *= 10;
        }
        if (pHysteresisTeslaX1000 != NULL) {
            *pHysteresisTeslaX1000 = (unsigned int) hysteresis;
        }

        result = ACTION_DRIVER_OK;
    }

    return result;
}

/**************************************************************************
 * PUBLIC FUNCTIONS: GENERIC
 *************************************************************************/

// Get the field strength in microTesla.
ActionDriver getFieldStrength(unsigned int *pTeslaX1000)
{
    ActionDriver result;
    int rawFieldStrength;
    char data[2];

    MTX_LOCK(gMtx);

    result = ACTION_DRIVER_ERROR_NOT_INITIALISED;

    if (gInitialised) {
        result = wakeUp();
        if (result == ACTION_DRIVER_OK) {
            result = ACTION_DRIVER_ERROR_I2C_WRITE_READ;
            // Read SI72XX_DSPSIGM
            data[0] = 0xc1; // SI72XX_DSPSIGM
            if (i2cSendReceive(gI2cAddress, data, 1, &(data[1]), 1) == 1) {
                // If the data is new, clear the old and read the other half in
                if ((data[1] & 0x80) != 0) {
                    rawFieldStrength = (((unsigned char) data[1]) & 0x7f) << 8;
                    data[0] = 0xC2; // SI72XX_DSPSIGL
                    if (i2cSendReceive(gI2cAddress, data, 1, &(data[1]), 1) == 1) {
                        rawFieldStrength += (unsigned char) data[1];
                        // 0x4000 is zero, the field being negative below this value
                        // and positive above, but we are only interested in the
                        // absolute value
                        rawFieldStrength -= 0x4000;
                        if (rawFieldStrength < 0) {
                            rawFieldStrength = -rawFieldStrength;
                        }
                        gRawFieldStrength = (unsigned int) rawFieldStrength;
                        result = ACTION_DRIVER_OK;
                    }
                } else {
                    // Just return the existing data
                    result = ACTION_DRIVER_OK;
                }
            }

            if ((result == ACTION_DRIVER_OK) && (pTeslaX1000 != NULL)) {
                switch (gRange) {
                    case SI7210_RANGE_20_MILLI_TESLAS:
                        // gRawFieldStrength * 1.25
                        *pTeslaX1000 = (gRawFieldStrength / 4) + gRawFieldStrength;
                        result = ACTION_DRIVER_OK;
                    break;
                    case SI7210_RANGE_200_MILLI_TESLAS:
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

    MTX_UNLOCK(gMtx);

    return result;
}

// Get whether there has been an interrupt from the magnetometer.
bool getFieldStrengthInterruptFlag()
{
    return gTwasMe;
}

// Clear the magnetometer interrupt flag.
void clearFieldStrengthInterruptFlag()
{
    gTwasMe = false;
}

/**************************************************************************
 * PUBLIC FUNCTIONS: SI7210 SPECIFIC
 *************************************************************************/

// Initialise SI7210 hall effect sensor.
ActionDriver si7210Init(char i2cAddress)
{
    ActionDriver result;
    char data[2];

    MTX_LOCK(gMtx);

    result = ACTION_DRIVER_OK;

    if (!gInitialised) {
        gI2cAddress = i2cAddress;
        gpEventQueue = NULL;
        gpEventCallback = NULL;
        clearFieldStrengthInterruptFlag();

        result = wakeUp();
        if (result == ACTION_DRIVER_OK) {
            result = ACTION_DRIVER_ERROR_I2C_WRITE_READ;
            // Read the HW ID register, expecting chipid 1 and revid 4
            data[0] = 0xc0; // SI72XX_HREVID
            if (i2cSendReceive(gI2cAddress, data, 1, &(data[1]), 1) == 1) {
                if (data[1] == 0x14) {
                    // Set the range to default with the correct compensation
                    // parameters
                    gRange = SI7210_RANGE_20_MILLI_TESLAS;
                    result = copyCompensationParameters(0x21);
                    if (result == ACTION_DRIVER_OK) {
                        // Return to sleep with the measurement timer running
                        result = sleep(true);
                        if (result == ACTION_DRIVER_OK) {
                            gInitialised = true;
                        }
                    }
                } else {
                    result = ACTION_DRIVER_ERROR_DEVICE_NOT_PRESENT;
                }
            }
        }
    }

    // If anything goes wrong, leave the device (if it's there)
    // in its lowest power state.
    if (result != ACTION_DRIVER_OK) {
        sleep(false);
    }

    MTX_UNLOCK(gMtx);

    return result;
}

// Shut-down the SI7210 hall effect sensor.
void si7210Deinit()
{
    MTX_LOCK(gMtx);

    if (gInitialised) {
        wakeUp();
        sleep(false);
        gInitialised = false;
    }

    MTX_UNLOCK(gMtx);
}

// Set the field strength range.
ActionDriver si7210SetRange(Si7210FieldStrengthRange range)
{
    ActionDriver result;
    unsigned int threshold;
    unsigned int hysteresis;
    bool activeHigh;

    MTX_LOCK(gMtx);

    result = ACTION_DRIVER_ERROR_NOT_INITIALISED;

    if (gInitialised) {
        result = wakeUp();
        if (result == ACTION_DRIVER_OK) {
            if (range != gRange) {
                // When changing the range the meaning of the
                // interrupt settings changes, so first read
                // them out
                result = _getInterrupt(&threshold, &hysteresis, &activeHigh);
                if (result == ACTION_DRIVER_OK) {
                    switch (range) {
                        case SI7210_RANGE_20_MILLI_TESLAS:
                            // For 20 microTesla range the 6 parameters
                            // start at OTP address 0x21
                            result = copyCompensationParameters(0x21);
                        break;
                        case SI7210_RANGE_200_MILLI_TESLAS:
                            // For 200 microTesla range the 6 parameters
                            // start at OTP address 0x27
                            result = copyCompensationParameters(0x27);
                        break;
                        default:
                            result = ACTION_DRIVER_ERROR_PARAMETER;
                        break;
                    }
                }
                // Set the interrupt thresholds up once more.
                if (result == ACTION_DRIVER_OK) {
                    result = _setInterrupt(threshold, hysteresis, activeHigh);
                }
            }

            // Return to sleep with the measurement timer running
            sleep(true);
        }
    }

    if (result == ACTION_DRIVER_OK) {
        gRange = range;
    }

    MTX_UNLOCK(gMtx);

    return result;
}

// Get the current measurement range.
Si7210FieldStrengthRange si7210GetRange()
{
    return gRange;
}

// Set up the interrupt.
ActionDriver si7210SetInterrupt(unsigned int thresholdTeslaX1000,
                                 unsigned int hysteresisTeslaX1000,
                                 bool activeHigh,
                                 EventQueue *pEventQueue,
                                 void (*pEventCallback) (EventQueue *))
{
    ActionDriver result;

    MTX_LOCK(gMtx);

    result = ACTION_DRIVER_ERROR_NOT_INITIALISED;

    if (gInitialised) {
        gpEventQueue = pEventQueue;
        gpEventCallback = pEventCallback;
        result = wakeUp();
        if (result == ACTION_DRIVER_OK) {
            result = _setInterrupt(thresholdTeslaX1000,
                                   hysteresisTeslaX1000,
                                   activeHigh);
            if (result == ACTION_DRIVER_OK) {
                if (activeHigh) {
                    gInterrupt.rise(interruptCallback);
                    gInterrupt.enable_irq();
                } else {
                    gInterrupt.fall(interruptCallback);
                    gInterrupt.disable_irq();
                }
            }
        }

        // Return to sleep with the measurement timer running
        sleep(true);
    }

    MTX_UNLOCK(gMtx);

    return result;
}

// Get the interrupt.
ActionDriver si7210GetInterrupt(unsigned int *pThresholdTeslaX1000,
                                unsigned int *pHysteresisTeslaX1000,
                                bool *pActiveHigh)
{
    ActionDriver result;

    MTX_LOCK(gMtx);

    result = ACTION_DRIVER_ERROR_NOT_INITIALISED;

    if (gInitialised) {
        result = wakeUp();
        if (result == ACTION_DRIVER_OK) {
            result = _getInterrupt(pThresholdTeslaX1000,
                                   pHysteresisTeslaX1000,
                                   pActiveHigh);
            // Return to sleep with the measurement timer running
            sleep(true);
        }
    }

    MTX_UNLOCK(gMtx);

    return result;
}

// End of file
