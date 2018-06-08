/* The code here is borrowed from:
 *
 * https://os.mbed.com/teams/SiliconLabs/code/Si1133//#1e2dd643afa8
 *
 * All rights remain with the original author(s).
 */

#include <mbed.h>
#include <eh_debug.h>
#include <eh_i2c.h>
#include <act_position.h>
#include <act_zoem8.h>

/**************************************************************************
 * MANIFEST CONSTANTS
 *************************************************************************/

/**************************************************************************
 * TYPES
 *************************************************************************/

/**************************************************************************
 * LOCAL VARIABLES
 *************************************************************************/

/** Flag so that we know if we've been initialised.
 */
static bool gInitialised = false;

/** The I2C address of the Zoe M8.
 */
static char gI2cAddress = 0;

/**************************************************************************
 * STATIC FUNCTIONS
 *************************************************************************/

/**************************************************************************
 * PUBLIC FUNCTIONS
 *************************************************************************/

// Initialise the Zoe M8 GNSS chip.
// TODO proper configuration for lower power mode.
ActionDriver zoem8Init(char i2cAddress)
{
    ActionDriver result = ACTION_DRIVER_OK;
    char data[2];

    gI2cAddress = i2cAddress;

    return result;
}

// Shut-down the Zoe M8 GNSS chip.
void zoem8Deinit()
{
    // TODO
    gInitialised = false;
}

// Read the position
ActionDriver getPosition(int *pLatitudeX1000, int *pLongitudeX1000,
                         int *pRadiusMetres, int *pAltitudeMetres,
                         unsigned char *pSpeedMPS)
{
    ActionDriver result = ACTION_DRIVER_ERROR_NOT_INITIALISED;
    char data[2];

    if (gInitialised) {
    }

    return result;
}

// End of file
