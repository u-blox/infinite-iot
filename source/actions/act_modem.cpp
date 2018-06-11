/* The code here is borrowed from:
 *
 * https://os.mbed.com/users/MACRUM/code/BME280/#c1f1647004c4
 *
 * All rights remain with the original author(s).
 */

#include <mbed.h>
#include "UbloxATCellularInterfaceN2xx.h"
#include "UbloxATCellularInterface.h"
#include <eh_debug.h>
#include <eh_config.h>
#include <act_cellular.h>
#include <act_modem.h>

/**************************************************************************
 * MANIFEST CONSTANTS
 *************************************************************************/

/**************************************************************************
 * LOCAL VARIABLES
 *************************************************************************/

// Output pin to switch on power to the cellular modem.
static DigitalOut gEnableCdc(PIN_ENABLE_CDC);

// Output pin to *signal* power to the cellular mdoem.
static DigitalOut gCpOn(PIN_CP_ON);

// Output pin to reset the cellular modem.
static DigitalOut gCpResetBar(PIN_CP_RESET_BAR);

/** Pointer to the cellular interface driver.
 */
void *gpInterface = NULL;

/** Flag to indicate the type of modem that is attached.
 */
static bool gUseN2xxModem = false;

/**************************************************************************
 * STATIC FUNCTIONS
 *************************************************************************/

#ifdef __cplusplus
extern "C" {
#endif

void onboard_modem_init()
{
    if (gUseN2xxModem) {
        // Turn the power off and on again,
        // there is no reset line
        gEnableCdc = 0;
        wait_ms(500);
        gEnableCdc = 1;
    } else {
        // Take us out of reset
        gCpResetBar = 1;
    }
}

void onboard_modem_deinit()
{
    if (gUseN2xxModem) {
        // Nothing to do
    } else {
        // Back into reset
        gCpResetBar = 0;
    }
}

void onboard_modem_power_up()
{
    // Power on
    gEnableCdc = 1;
    wait_ms(50);

    if (!gUseN2xxModem) {
        // Keep the power-signal line low for 1 second
        gCpOn = 0;
        wait_ms(1000);
        gCpOn = 1;
        // Give modem a little time to respond
        wait_ms(100);
    }
}

void onboard_modem_power_down()
{
    // Power off
    gEnableCdc = 0;
}

#ifdef __cplusplus
}
#endif

/**************************************************************************
 * PUBLIC FUNCTIONS
 *************************************************************************/

// Initialise the modem.
ActionDriver modemInit()
{
    ActionDriver result = ACTION_DRIVER_OK;

    if (gpInterface != NULL) {
        // The N2xx cellular modem uses no power-on lines, it just
        // powers up, so try running that driver first: if it works
        // then an N2xx cellular modem is definitely attached.
        gpInterface = new UbloxATCellularInterfaceN2xx();
        if (gpInterface != NULL) {
            ((UbloxATCellularInterfaceN2xx *) gpInterface)->set_credentials(APN, USERNAME, PASSWORD);
            ((UbloxATCellularInterfaceN2xx *) gpInterface)->set_network_search_timeout(CELLULAR_CONNECT_TIMEOUT_SECONDS);
            ((UbloxATCellularInterfaceN2xx *) gpInterface)->set_release_assistance(true);
            gUseN2xxModem = ((UbloxATCellularInterfaceN2xx *) gpInterface)->init(SIM_PIN);
            if (!gUseN2xxModem) {
                // If that didn't work, try the R410M driver
                delete (UbloxATCellularInterface *) gpInterface;
                gpInterface = new UbloxATCellularInterface();
                if (gpInterface != NULL) {
                    ((UbloxATCellularInterface *) gpInterface)->set_credentials(APN, USERNAME, PASSWORD);
                    ((UbloxATCellularInterface *) gpInterface)->set_network_search_timeout(CELLULAR_CONNECT_TIMEOUT_SECONDS);
                    ((UbloxATCellularInterface *) gpInterface)->set_release_assistance(true);
                    if (!((UbloxATCellularInterface *) gpInterface)->init(SIM_PIN)) {
                        delete (UbloxATCellularInterface *) gpInterface;
                        gpInterface = NULL;
                        result = ACTION_DRIVER_ERROR_DEVICE_NOT_PRESENT;
                    }
                } else {
                    result = ACTION_DRIVER_ERROR_OUT_OF_MEMORY;
                }
            }
        } else {
            result = ACTION_DRIVER_ERROR_OUT_OF_MEMORY;
        }
    }

    return result;
}

// Shut-down the modem.
void modemDeinit()
{
    if (gpInterface != NULL) {
        if (gUseN2xxModem) {
            ((UbloxATCellularInterfaceN2xx *) gpInterface)->disconnect();
            ((UbloxATCellularInterfaceN2xx *) gpInterface)->deinit();
            delete (UbloxATCellularInterfaceN2xx *) gpInterface;
        } else {
            ((UbloxATCellularInterface *) gpInterface)->disconnect();
            ((UbloxATCellularInterface *) gpInterface)->deinit();
            delete (UbloxATCellularInterface *) gpInterface;
        }
        gpInterface = NULL;
    }
}

// Make a data connection.
ActionDriver modemConnect()
{
    ActionDriver result = ACTION_DRIVER_ERROR_NOT_INITIALISED;
    bool connected = false;

    if (gpInterface != NULL) {
        if (gUseN2xxModem) {
            connected = (((UbloxATCellularInterfaceN2xx *) gpInterface)->connect() == 0);
        } else {
            connected = (((UbloxATCellularInterface *) gpInterface)->connect() == 0);
        }

        if (connected) {
            result = ACTION_DRIVER_OK;
        }

    }

    return result;
}

// Get the time from an NTP server.
ActionDriver modemGetTime(time_t *pTime)
{
    ActionDriver result = ACTION_DRIVER_ERROR_NOT_INITIALISED;

    if (gpInterface != NULL) {
        // TODO
    }

    return result;
}

// Send reports.
ActionDriver modemSendReports()
{
    ActionDriver result = ACTION_DRIVER_ERROR_NOT_INITIALISED;

    if (gpInterface != NULL) {
        // TODO
    }

    return result;
}

//  Get the received signal strengths
ActionDriver getSignalStrengthRx(int *pRsrpDbm, int *pRssiDbm,
                                 int *pRsrq, int *pSnrDbm, int *pEclDbm)
{
    ActionDriver result = ACTION_DRIVER_ERROR_NOT_INITIALISED;

    if (gpInterface != NULL) {
        // TODO
    }

    return result;
}

// Get the transmit signal strength.
ActionDriver getSignalStrengthTx(int *pPowerDbm)
{
    ActionDriver result = ACTION_DRIVER_ERROR_NOT_INITIALISED;

    if (gpInterface != NULL) {
        // TODO
    }

    return result;
}

// Get the channel parameters.
ActionDriver getChannel(int *pPhysicalCellId, int *pPci, int *pEarfcn)
{
    ActionDriver result = ACTION_DRIVER_ERROR_NOT_INITIALISED;

    if (gpInterface != NULL) {
        // TODO
    }

    return result;
}

// End of file
