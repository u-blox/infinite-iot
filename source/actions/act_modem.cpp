/* The code here is borrowed from:
 *
 * https://os.mbed.com/users/MACRUM/code/BME280/#c1f1647004c4
 *
 * All rights remain with the original author(s).
 */

#include <mbed.h>
#include <UbloxATCellularInterfaceN2xx.h>
#include <UbloxATCellularInterface.h>
#include <eh_utilities.h> // for MTX_LOCK()/MTX_UNLOCK()
#include <eh_debug.h>
#include <eh_config.h>
#include <eh_statistics.h>
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

/** Mutex to protect the against multiple accessors.
 */
static Mutex gMtx;

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
    ActionDriver result;

    MTX_LOCK(gMtx);

    result = ACTION_DRIVER_OK;

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

    MTX_UNLOCK(gMtx);

    return result;
}

// Shut-down the modem.
void modemDeinit()
{
    MTX_LOCK(gMtx);

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

    MTX_UNLOCK(gMtx);
}

// Get the IMEI from the modem.
ActionDriver modemGetImei(char *pImei)
{
     ActionDriver result;
     const char *pString;

     MTX_LOCK(gMtx);

     result = ACTION_DRIVER_ERROR_NOT_INITIALISED;

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

     MTX_UNLOCK(gMtx);

     return result;
}

// Make a data connection.
ActionDriver modemConnect()
{
    ActionDriver result;
    bool connected = false;

    MTX_LOCK(gMtx);

    result = ACTION_DRIVER_ERROR_NOT_INITIALISED;

    if (gpInterface != NULL) {
        statisticsIncConnectionAttempts();
        if (gUseN2xxModem) {
            connected = (((UbloxATCellularInterfaceN2xx *) gpInterface)->connect() == 0);
        } else {
            connected = (((UbloxATCellularInterface *) gpInterface)->connect() == 0);
        }

        if (connected) {
            statisticsIncConnectionSuccess();
            result = ACTION_DRIVER_OK;
        }

    }

    MTX_UNLOCK(gMtx);

    return result;
}

// Get the time from an NTP server.
ActionDriver modemGetTime(time_t *pTimeUTC)
{
    ActionDriver result;
    UDPSocket sockUdp;
    SocketAddress udpServer;
    SocketAddress udpSenderAddress;
    time_t timeUTC;
    int x;

    MTX_LOCK(gMtx);

    result = ACTION_DRIVER_ERROR_NOT_INITIALISED;
    timeUTC = 0;

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
                    statisticsAddTransmitted(48);
                    result = ACTION_DRIVER_ERROR_NO_VALID_DATA;
                    x = sockUdp.recvfrom(&udpSenderAddress, gBuf, sizeof (gBuf));
                    // If there's enough data, it's a response
                    if (x >= 43) {
                        statisticsAddReceived(x);
                        timeUTC |= ((int) *(gBuf + 40)) << 24;
                        timeUTC |= ((int) *(gBuf + 41)) << 16;
                        timeUTC |= ((int) *(gBuf + 42)) << 8;
                        timeUTC |= ((int) *(gBuf + 43));
                        timeUTC -= 2208988800U; // 2208988800U is TIME1970
                        if (pTimeUTC != NULL) {
                            *pTimeUTC = timeUTC;
                        }
                        result = ACTION_DRIVER_OK;
                    }
                }
                sockUdp.close();
            }
        }
    }

    MTX_UNLOCK(gMtx);

    return result;
}

// Send reports.
ActionDriver modemSendReports(const char *pServerAddress, int serverPort,
                              const char *pIdString)
{
    ActionDriver result;
    UDPSocket sockUdp;
    SocketAddress udpServer;
    SocketAddress udpSenderAddress;
    Timer ackTimeout;
    bool gotAck;
    int x;
    int y;

    MTX_LOCK(gMtx);

    result = ACTION_DRIVER_ERROR_NOT_INITIALISED;

    if (gpInterface != NULL) {
        result = ACTION_DRIVER_ERROR_PARAMETER;
        if (gUseN2xxModem) {
            x = ((UbloxATCellularInterfaceN2xx *) gpInterface)->gethostbyname(pServerAddress, &udpServer);
        } else {
            x = ((UbloxATCellularInterface *) gpInterface)->gethostbyname(pServerAddress, &udpServer);
        }

        if (x == 0) {
            result = ACTION_DRIVER_ERROR_OUT_OF_MEMORY;
            udpServer.set_port(serverPort);
            if (sockUdp.open(gpInterface) == 0) {
                sockUdp.set_timeout(SOCKET_TIMEOUT_MS);

                // Encode and send data until done
                codecPrepareData();
                while (CODEC_SIZE(x = codecEncodeData(pIdString, gBuf, sizeof(gBuf))) > 0) {
                    result = ACTION_DRIVER_OK;
                    MBED_ASSERT((CODEC_FLAGS(x) &
                                 (CODEC_FLAG_NOT_ENOUGH_ROOM_FOR_HEADER |
                                  CODEC_FLAG_NOT_ENOUGH_ROOM_FOR_EVEN_ONE_DATA)) == 0);
                    if (sockUdp.sendto(udpServer, (void *) gBuf, CODEC_SIZE(x)) == CODEC_SIZE(x)) {
                        statisticsAddTransmitted(CODEC_SIZE(x));
                        if ((CODEC_FLAGS(x) & CODEC_FLAG_NEEDS_ACK) != 0) {
                            ackTimeout.reset();
                            ackTimeout.start();
                            // Wait for the ack and re-send as necessary
                            gotAck = false;
                            while (!gotAck && (ackTimeout.read_ms() < ACK_TIMEOUT_MS)) {
                                if ((y = sockUdp.recvfrom(&udpSenderAddress, (void *) gAckBuf, sizeof(gAckBuf))) > 0) {
                                    statisticsAddReceived(y);
                                    if ((codecGetLastIndex() == codecDecodeAck(gAckBuf, y, pIdString))) {
                                        // Got an ack for the last index so ack all the data up to this point
                                        // in the data queue.
                                        codecAckData();
                                        gotAck = true;
                                    }
                                }
                                if (!gotAck) {
                                    // Try sending again
                                    sockUdp.sendto(udpServer, (void *) gBuf, CODEC_SIZE(x));
                                }
                            }
                            ackTimeout.stop();
                            // Note: if no ack is received within the timeout then the data
                            // that requires an ack will remain in the queue and will be
                            // transmitted again on the next call to send reports
                        }
                    } else {
                        result = ACTION_DRIVER_ERROR_SEND_REPORTS;
                    }
                }

                sockUdp.close();
            }
        }
    }

    MTX_UNLOCK(gMtx);

    return result;
}

//  Get the received signal strengths
ActionDriver getSignalStrengthRx(int *pRsrpDbm, int *pRssiDbm,
                                 int *pRsrq, int *pSnrDbm, int *pEclDbm)
{
    ActionDriver result;

    MTX_LOCK(gMtx);

    result = ACTION_DRIVER_ERROR_NOT_INITIALISED;

    if (gpInterface != NULL) {
        // TODO
    }

    MTX_UNLOCK(gMtx);

    return result;
}

// Get the transmit signal strength.
ActionDriver getSignalStrengthTx(int *pPowerDbm)
{
    ActionDriver result;

    MTX_LOCK(gMtx);

    result = ACTION_DRIVER_ERROR_NOT_INITIALISED;

    if (gpInterface != NULL) {
        // TODO
    }

    MTX_UNLOCK(gMtx);

    return result;
}

// Get the channel parameters.
ActionDriver getChannel(unsigned int *pPhysicalCellId,
                        unsigned int *pPci,
                        unsigned int *pEarfcn)
{
    ActionDriver result;

    MTX_LOCK(gMtx);

    result = ACTION_DRIVER_ERROR_NOT_INITIALISED;

    if (gpInterface != NULL) {
        // TODO
    }

    MTX_UNLOCK(gMtx);

    return result;
}

// End of file
