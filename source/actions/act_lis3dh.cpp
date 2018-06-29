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

/**************************************************************************
 * STATIC FUNCTIONS
 *************************************************************************/

/**************************************************************************
 * PUBLIC FUNCTIONS
 *************************************************************************/

// Initialise LIS3DH orientation sensor.
// TODO set thresholds to return to sleep
// TODO set up interrupt
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
// TODO check this.
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

// End of file
