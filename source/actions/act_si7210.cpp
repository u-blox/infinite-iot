/* The code here is borrowed from:
 *
 * https://os.mbed.com/users/stevew817/code/Si7210/#9bce98da584b
 *
 * All rights remain with the original authors.
 */

#include <mbed.h>
#include <eh_debug.h>
#include <eh_i2c.h>
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

/**************************************************************************
 * STATIC FUNCTIONS
 *************************************************************************/

/**************************************************************************
 * PUBLIC FUNCTIONS
 *************************************************************************/

// Initialise SI7210 hall effect sensor.
ActionDriver si7210Init(char i2cAddress)
{
    ActionDriver result = ACTION_DRIVER_OK;
    return result;
}

// Shut-down the SI7210 hall effect sensor.
void si7210Deinit()
{
    // TODO
    gInitialised = false;
}

// End of file
