/* The code here is borrowed from:
 *
 * https://os.mbed.com/teams/SiliconLabs/code/Si1133//#1e2dd643afa8
 *
 * All rights remain with the original authors.
 */

#include <mbed.h>
#include <eh_debug.h>
#include <eh_i2c.h>
#include <act_si1133.h>

/**************************************************************************
 * MANIFEST CONSTANTS
 *************************************************************************/

/**************************************************************************
 * LOCAL VARIABLES
 *************************************************************************/

/** Flag so that we know if we've been initialised.
 */
static bool gInitialised = false;

/** The I2C address of the SI1133.
 */
static char gI2cAddress = 0;

/**************************************************************************
 * STATIC FUNCTIONS
 *************************************************************************/

/**************************************************************************
 * PUBLIC FUNCTIONS
 *************************************************************************/

// Initialise SI1133 light sensor.
ActionDriver si1133Init(char i2cAddress)
{
    ActionDriver result = ACTION_DRIVER_OK;
    return result;
}

// Shut-down the SI1133 light sensor.
void si1133Deinit()
{
    // TODO
    gInitialised = false;
}

// End of file
