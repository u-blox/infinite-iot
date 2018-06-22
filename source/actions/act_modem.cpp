/* The code here is borrowed from:
 *
 * https://os.mbed.com/users/MACRUM/code/BME280/#c1f1647004c4
 *
 * All rights remain with the original author(s).
 */

#include <mbed.h>
#include <UbloxATCellularInterfaceN2xx.h>
#include <UbloxATCellularInterface.h>
#include <eh_debug.h>
#include <eh_config.h>
#include <eh_codec.h>
#include <act_cellular.h>
#include <act_modem.h>

/**************************************************************************
 * MANIFEST CONSTANTS
 *************************************************************************/

/**************************************************************************
 * LOCAL VARIABLES
 *************************************************************************/

/** Output pin to switch on power to the cellular modem.
 */
static DigitalOut gEnableCdc(PIN_ENABLE_CDC);

/** Output pin to *signal* power to the cellular mdoem.
 */
static DigitalOut gCpOn(PIN_CP_ON);

/** Pointer to the cellular interface driver.
 */
static void *gpInterface = NULL;

/** Flag to indicate that we have been initialised at least once
 * (and therefore figured out what modem is attached).
 */
static bool gInitialisedOnce = false;

/** Flag to indicate the type of modem that is attached.
 */
static bool gUseN2xxModem = false;

/** General buffer for exchanging data with a server.
 */
static char gBuf[CODEC_ENCODE_BUFFER_MIN_SIZE];

/** Separate buffer for encoding JSON coded acks received
 * from the server.
 */
static char gAckBuf[CODEC_DECODE_BUFFER_MIN_SIZE];

/**************************************************************************
 * STATIC FUNCTIONS
 *************************************************************************/

#ifndef TARGET_UBLOX_C030

#ifdef __cplusplus
extern "C" {
#endif

void onboard_modem_init()
{
    // Nothing to do
}

void onboard_modem_deinit()
{
    // Nothing to do
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

#endif

// Instantiate a SARA-N2 modem
static void *pGetSaraN2(const char *pSimPin, const char *pApn,
                        const char *pUserName, const char *pPassword)
{
    UbloxATCellularInterfaceN2xx *pInterface = new UbloxATCellularInterfaceN2xx(MDMTXD,
                                                                                MDMRXD,
                                                                                MBED_CONF_UBLOX_CELL_N2XX_BAUD_RATE/*,
                                                                                MBED_CONF_APP_ENABLE_PRINTF*/);
    gUseN2xxModem = true;

    if (pInterface != NULL) {
        pInterface->set_credentials(pApn, pUserName, pPassword);
        pInterface->set_network_search_timeout(CELLULAR_CONNECT_TIMEOUT_SECONDS);
        pInterface->set_release_assistance(true);
        gUseN2xxModem = pInterface->init(pSimPin);
        if (!gUseN2xxModem) {
            delete pInterface;
            pInterface = NULL;
        }
    }

    return pInterface;
}

// Instantiate a SARA-R4 modem
static void *pGetSaraR4(const char *pSimPin, const char *pApn,
                        const char *pUserName, const char *pPassword)
{
    UbloxATCellularInterface *pInterface = new UbloxATCellularInterface(MDMTXD,
                                                                        MDMRXD,
                                                                        MBED_CONF_UBLOX_CELL_BAUD_RATE/*,
                                                                        MBED_CONF_APP_ENABLE_PRINTF*/);

    if (pInterface != NULL) {
        pInterface->set_credentials(pApn, pUserName, pPassword);
        pInterface->set_network_search_timeout(CELLULAR_CONNECT_TIMEOUT_SECONDS);
        pInterface->set_release_assistance(true);
        if (!pInterface->init(pSimPin)) {
            delete pInterface;
            pInterface = NULL;
        }
    }

    return pInterface;
}

/**************************************************************************
 * PUBLIC FUNCTIONS
 *************************************************************************/

// Initialise the modem.
ActionDriver modemInit(const char *pSimPin, const char *pApn,
                       const char *pUserName, const char *pPassword)
{
    ActionDriver result = ACTION_DRIVER_OK;

    if (gpInterface == NULL) {
#if MBED_CONF_APP_FORCE_R4_MODEM
        gInitialisedOnce = true;
        gUseN2xxModem = false;
#else
# if MBED_CONF_APP_FORCE_N2_MODEM
        gInitialisedOnce = true;
        gUseN2xxModem = true;
# endif
#endif
        // If we've been initialised once, just instantiate the right modem
        if (gInitialisedOnce) {
            if (gUseN2xxModem) {
                gpInterface = pGetSaraN2(pSimPin, pApn, pUserName, pPassword);
            } else {
                gpInterface = pGetSaraR4(pSimPin, pApn, pUserName, pPassword);
            }
        } else {
            // The N2xx cellular modem uses no power-on lines, it just
            // powers up, so try running that driver first: if it works
            // then an N2xx cellular modem is definitely attached.
            gpInterface = pGetSaraN2(pSimPin, pApn, pUserName, pPassword);
            if (gpInterface == NULL) {
                // If that didn't work, try the R410M driver
                gpInterface = pGetSaraR4(pSimPin, pApn, pUserName, pPassword);
            }
        }

        if (gpInterface != NULL) {
            gInitialisedOnce = true;
        } else {
            result = ACTION_DRIVER_ERROR_DEVICE_NOT_PRESENT;
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

// Get the IMEI from the modem.
ActionDriver modemGetImei(char *pImei)
{
     ActionDriver result = ACTION_DRIVER_ERROR_NOT_INITIALISED;
     const char *pString;

     if (gpInterface != NULL) {

         if (gUseN2xxModem) {
             pString = ((UbloxATCellularInterfaceN2xx *) gpInterface)->imei();
         } else {
             pString = ((UbloxATCellularInterface *) gpInterface)->imei();
         }

         if (pImei != NULL) {
             memset(pImei, 0, MODEM_IMEI_LENGTH);
             memcpy(pImei, pString, MODEM_IMEI_LENGTH - 1);
         }

         result = ACTION_DRIVER_OK;
     }

     return result;
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
ActionDriver modemGetTime(time_t *pTimeUtc)
{
    ActionDriver result = ACTION_DRIVER_ERROR_NOT_INITIALISED;
    UDPSocket sockUdp;
    SocketAddress udpServer;
    SocketAddress udpSenderAddress;
    time_t timeUtc = 0;
    int x;

    if (gpInterface != NULL) {
        result = ACTION_DRIVER_ERROR_PARAMETER;
        if (gUseN2xxModem) {
            x = ((UbloxATCellularInterfaceN2xx *) gpInterface)->gethostbyname(NTP_SERVER_IP_ADDRESS, &udpServer);
        } else {
            x = ((UbloxATCellularInterface *) gpInterface)->gethostbyname(NTP_SERVER_IP_ADDRESS, &udpServer);
        }

        if (x == 0){
            result = ACTION_DRIVER_ERROR_OUT_OF_MEMORY;
            udpServer.set_port(NTP_SERVER_PORT);
            if (sockUdp.open(gpInterface) == 0) {
                sockUdp.set_timeout(SOCKET_TIMEOUT_MS);
                memset (gBuf, 0, 48);
                *gBuf = '\x1b';
                // Send the request
                result = ACTION_DRIVER_ERROR_NO_DATA;
                if (sockUdp.sendto(udpServer, (void *) gBuf, 48) == 48) {
                    result = ACTION_DRIVER_ERROR_NO_VALID_DATA;
                    x = sockUdp.recvfrom(&udpSenderAddress, gBuf, sizeof (gBuf));
                    // If there's enough data, it's a response
                    if (x >= 43) {
                        timeUtc |= ((int) *(gBuf + 40)) << 24;
                        timeUtc |= ((int) *(gBuf + 41)) << 16;
                        timeUtc |= ((int) *(gBuf + 42)) << 8;
                        timeUtc |= ((int) *(gBuf + 43));
                        timeUtc -= 2208988800U; // 2208988800U is TIME1970
                        if (pTimeUtc != NULL) {
                            *pTimeUtc = timeUtc;
                        }
                        result = ACTION_DRIVER_OK;
                    }
                }
                sockUdp.close();
            }
        }
    }

    return result;
}

// Send reports.
ActionDriver modemSendReports(const char *pIdString)
{
    ActionDriver result = ACTION_DRIVER_ERROR_NOT_INITIALISED;
    UDPSocket sockUdp;
    SocketAddress udpServer;
    SocketAddress udpSenderAddress;
    Timer ackTimeout;
    int x;

    if (gpInterface != NULL) {
        result = ACTION_DRIVER_ERROR_PARAMETER;
        if (gUseN2xxModem) {
            x = ((UbloxATCellularInterfaceN2xx *) gpInterface)->gethostbyname(IOT_SERVER_IP_ADDRESS, &udpServer);
        } else {
            x = ((UbloxATCellularInterface *) gpInterface)->gethostbyname(IOT_SERVER_IP_ADDRESS, &udpServer);
        }

        if (x == 0){
            result = ACTION_DRIVER_ERROR_OUT_OF_MEMORY;
            udpServer.set_port(IOT_SERVER_PORT);
            if (sockUdp.open(gpInterface) == 0) {
                sockUdp.set_timeout(SOCKET_TIMEOUT_MS);

                // Encode and send data until done
                codecPrepareData();
                while (CODEC_SIZE(x = codecEncodeData(pIdString, gBuf, sizeof(gBuf))) > 0) {
                    MBED_ASSERT((CODEC_FLAGS(x) &
                                 (CODEC_FLAG_NOT_ENOUGH_ROOM_FOR_HEADER |
                                  CODEC_FLAG_NOT_ENOUGH_ROOM_FOR_EVEN_ONE_DATA)) == 0);
                    if (sockUdp.sendto(udpServer, (void *) gBuf, CODEC_SIZE(x)) == CODEC_SIZE(x)) {
                        // Wait for ack if required
                        if ((CODEC_FLAGS(x) & CODEC_FLAG_NEEDS_ACK) != 0) {
                            ackTimeout.reset();
                            ackTimeout.start();
                            // Wait for the ack and resend as necessary
                            while (ackTimeout.read_ms() < ACK_TIMEOUT_MS) {
                                if (((x = sockUdp.recvfrom(&udpSenderAddress, (void *) gAckBuf, sizeof(gAckBuf))) > 0) &&
                                     (codecGetLastIndex() == codecDecodeAck(gAckBuf, x, pIdString))) {
                                    // Got an ack for the last index so ack all the data up to this point
                                    // in the data queue.
                                    codecAckData();
                                } else {
                                    // Try sending again
                                    sockUdp.sendto(udpServer, (void *) gBuf, CODEC_SIZE(x));
                                }
                            }
                            ackTimeout.stop();
                            // Note: if no ack is received then the data will remain in the queue and
                            // will be transmitted again on the next call to send reports
                        }
                    }
                }

                sockUdp.close();
            }
        }
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
ActionDriver getChannel(unsigned int *pPhysicalCellId,
                        unsigned int *pPci,
                        unsigned int *pEarfcn)
{
    ActionDriver result = ACTION_DRIVER_ERROR_NOT_INITIALISED;

    if (gpInterface != NULL) {
        // TODO
    }

    return result;
}

// End of file
