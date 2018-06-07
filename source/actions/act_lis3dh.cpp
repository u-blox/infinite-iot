/* The code here is borrowed from:
 *
 * https://os.mbed.com/users/kenjiArai/code/LIS3DH/#0999d25ed7bc
 *
 * All rights remain with the original authors.
 */

#include <mbed.h>
#include <eh_debug.h>
#include <eh_i2c.h>
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

/**************************************************************************
 * STATIC FUNCTIONS
 *************************************************************************/

/**************************************************************************
 * PUBLIC FUNCTIONS
 *************************************************************************/

// Initialise LIS3DH orientation sensor.
ActionDriver lis3dhInit(char i2cAddress)
{
    ActionDriver result = ACTION_DRIVER_OK;
    return result;
}

// Shut-down the LIS3DH orientation sensor.
void lis3dhDeinit()
{
    // TODO
    gInitialised = false;
}

// End of file
