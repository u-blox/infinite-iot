
/* mbed Microcontroller Library
 * Copyright (c) 2006-2018 u-blox Limited
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <mbed.h>
#include <math.h> // for log10() and pow()
#include <errno.h>
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
static DigitalOut gEnableCdc(PIN_ENABLE_CDC, 0);

/** Output pin to *signal* power to the cellular modem.
 */
static DigitalOut *pgCpOn = NULL;

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

/** The last connection error code.
 */
static int gLastConnectErrorCode = 0;

/** Storage for the RSRP read from the modem.
 */
static int gRsrpDbm = 0;

/** Storage for the RSSI read from the modem.
 */
static int gRssiDbm = 0;

/** Storage for the RSRQ read from the modem.
 */
static int gRsrqDb = 0;

/** Storage for the SNR read from the modem.
*/
static int gSnrDb = 0;

/** Storage for the ECL read from the modem.
*/
static int gEcl = 0;

/** Storage for the TX power read from the modem.
 */
static int gTxPowerDbm = 0;

/** Storage for the cell ID read from the modem.
 */
static int gCellId = 0;

/** Storage for the EARFCN read from the modem.
 */
static int gEarfcn = 0;

/** General buffer for exchanging data with a server.
 */
static char gBuf[CODEC_ENCODE_BUFFER_MIN_SIZE];

/** Separate buffer for decoding JSON coded acks received
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
    Thread::wait(50);

    if (!gUseN2xxModem) {
#ifdef MODEM_IS_2G_3G
        // Just powering up is good enough
#else
        *pgCpOn = 0;
        // Keep the power-signal line low for more than 1 second
        Thread::wait(1200);
        *pgCpOn = 1;
#endif
        // Give modem a little time to respond
        Thread::wait(100);
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

// Callback for when a CME Error has occurred
// on the modem.
static void modemCmeErrorCallback(int errorNumber)
{
    AQ_NRG_LOG(EVENT_CME_ERROR, errorNumber);
}

#if !CELLULAR_N211_OFF_WHEN_NOT_IN_USE
// Callback for when the modem has entered power
// saving mode.
static void modemEnteredPsmCallback(void *pUnused)
{
    AQ_NRG_LOG(EVENT_MODEM_ENTERED_PSM, 0);
}
#endif

// Callback for the modem changes connection state.
static void modemCsconCallback(int state)
{
    AQ_NRG_LOG(EVENT_MODEM_CSCON_STATE, state);
}

// Return the modem interface to its off state.
static void modemInterfaceOff()
{
#if TARGET_UBLOX_EVK_NINA_B1
    // Use a direct call into the Nordic driver layer to set the
    // Tx and Rx pins to a default state which should prevent
    // current being drawn from them by the modem
    nrf_gpio_cfg(MDMTXD,
                 NRF_GPIO_PIN_DIR_OUTPUT,
                 NRF_GPIO_PIN_INPUT_DISCONNECT,
                 NRF_GPIO_PIN_NOPULL,
                 NRF_GPIO_PIN_S0D1,
                 NRF_GPIO_PIN_NOSENSE);

    nrf_gpio_cfg(MDMRXD,
                 NRF_GPIO_PIN_DIR_OUTPUT,
                 NRF_GPIO_PIN_INPUT_DISCONNECT,
                 NRF_GPIO_PIN_NOPULL,
                 NRF_GPIO_PIN_S0D1,
                 NRF_GPIO_PIN_NOSENSE);

    // Same for CP_ON or current will be drawn from that also
    if (pgCpOn != NULL) {
        delete pgCpOn;
        nrf_gpio_cfg(PIN_CP_ON,
                     NRF_GPIO_PIN_DIR_OUTPUT,
                     NRF_GPIO_PIN_INPUT_DISCONNECT,
                     NRF_GPIO_PIN_NOPULL,
                     NRF_GPIO_PIN_S0D1,
                     NRF_GPIO_PIN_NOSENSE);
    }

    // Make sure power is really off
    gEnableCdc = 0;
#endif
}

// Instantiate a SARA-N2 modem
static void *pGetSaraN2(const char *pSimPin, const char *pApn,
                        const char *pUserName, const char *pPassword)
{
    UbloxATCellularInterfaceN2xx *pInterface = new UbloxATCellularInterfaceN2xx(MDMTXD,
                                                                                MDMRXD,
#if CELLULAR_N211_OFF_WHEN_NOT_IN_USE
// Can run the serial port at a higher rate (but not quite 115200) if we're not power saving
                                                                                57600,
#else
                                                                                MBED_CONF_UBLOX_CELL_N2XX_BAUD_RATE,
#endif
                                                                                MODEM_DEBUG);
    if (pInterface != NULL) {
        pInterface->set_credentials(pApn, pUserName, pPassword);
        // Best to have this off if we're not going into power saving
        // (so that we don't keep dropping in and out of an RRC connection
        // when sending stuff) and on if we are going to leave the modem
        // on afterwards (when we don't want power wasted at the end)
        pInterface->set_release_assistance(!CELLULAR_N211_OFF_WHEN_NOT_IN_USE);
        pInterface->set_cme_error_callback(modemCmeErrorCallback);
        pInterface->set_cscon_callback(modemCsconCallback);
        if (pInterface->init(pSimPin)) {
#if !CELLULAR_N211_OFF_WHEN_NOT_IN_USE
            pInterface->set_power_saving_mode(CELLULAR_PERIODIC_TAU_TIME_SECONDS,
                                              CELLULAR_ACTIVE_TIME_SECONDS,
                                              modemEnteredPsmCallback);
#endif
        } else {
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
                                                                        MBED_CONF_UBLOX_CELL_BAUD_RATE,
                                                                        MODEM_DEBUG);

    if (pInterface != NULL) {
        pInterface->set_credentials(pApn, pUserName, pPassword);
        // Best to have this off if we're not going into power saving
        // (so that we don't keep dropping in and out of an RRC connection
        // when sending stuff) and on if we are going to leave the modem
        // on afterwards (when we don't want power wasted at the end)
        pInterface->set_release_assistance(!CELLULAR_N211_OFF_WHEN_NOT_IN_USE);
        pInterface->set_cme_error_callback(modemCmeErrorCallback);
        pInterface->set_cscon_callback(modemCsconCallback);
        pInterface->set_radio_config(CELLULAR_R4_RAT,
                                     CELLULAR_R4_BAND_MASK);
        if (!pInterface->init(pSimPin)) {
            delete pInterface;
            pInterface = NULL;
        }
    }

    return pInterface;
}

// Retrieve NUESTATS from a SARA-N2xx module.
static bool getNUEStats()
{
    bool success;

    MBED_ASSERT(gUseN2xxModem);

    success = ((UbloxATCellularInterfaceN2xx *) gpInterface)->get_nuestats(&gRsrpDbm,
                                                                           &gRssiDbm,
                                                                           &gTxPowerDbm,
                                                                           NULL,
                                                                           NULL,
                                                                           &gCellId,
                                                                           &gEcl,
                                                                           &gSnrDb,
                                                                           &gEarfcn,
                                                                           NULL,
                                                                           &gRsrqDb);
    if (success) {
        // Answers for these values are in 10ths of a dB so convert them here
        gRsrpDbm /= 10;
        gRssiDbm /= 10;
        gTxPowerDbm /= 10;
    }

    return success;
}

// Convert RxLev to RSSI.  Returns 0 if the number is not known.
// 0: less than -110 dBm,
// 1..62: from -110 to -49 dBm with 1 dBm steps,
// 63: -48 dBm or greater,
// 99: not known or not detectable.
static int rxLevToRssiDbm(int rxLev)
{
    int rssiDbm = 0;

    if (rxLev <= 63) {
        rssiDbm = rxLev - 63 - 48;
        if (rssiDbm < -110) {
            rssiDbm = -110;
        }
    }

    return rssiDbm;
}

// Convert RSRQ to dB as a whole number.  Returns 0 if the number is not known.
// 0: -19 dB or less,
// 1..33: from -19.5 dB to -3.5 dB with 0.5 dB steps,
// 34: -3 dB or greater,
// 255: not known or not detectable.
static int rsrqToDb(int rsrq)
{
    int rsrqDb = 0;

    if (rsrq <= 34) {
        rsrqDb = (rsrq - 34 - 6) / 2;
        if (rsrqDb < -19) {
            rsrqDb = -19;
        }
    }

    return rsrqDb;
}

// Convert RSRP to dBm.  Returns 0 if the number is not known.
// 0: -141 dBm or less,
// 1..96: from -140 dBm to -45 dBm with 1 dBm steps,
// 97: -44 dBm or greater,
// 255: not known or not detectable.
static int rsrpToDbm(int rsrp)
{
    int rsrpDbm = 0;

    if (rsrp <= 97) {
        rsrpDbm = rsrp - 97 - 44;
        if (rsrpDbm < -141) {
            rsrpDbm = -141;
        }
    }

    return rsrpDbm;
}

// Work out SNR from RSSI and RSRP.  SNR in dB is returned
// in the 3rd parameter and true is returned on success.
// SNR = RSRP / (RSSI - RSRP).
static bool snrDb(int rssiDbm, int rsrpDbm, int *pSnrDb)
{
    bool success = false;
    double rssi;
    double rsrp;
    double snrDb;

    // First convert from dBm
    rssi = pow(10.0, ((double) rssiDbm) / 10);
    rsrp = pow(10.0, ((double) rsrpDbm) / 10);

    if (errno == 0) {
        snrDb = 10 * log10(rsrp / (rssi - rsrp));
        if (errno == 0) {
            *pSnrDb = (int) snrDb;
            success = true;
        }
    }

    return success;
}

// Retrieve the data that AT+CESQ provides
static bool getCESQ()
{
    int rxlev;
    int rsrq;
    int rsrp;
    bool success;

    MBED_ASSERT(!gUseN2xxModem);

    success = ((UbloxATCellularInterface *) gpInterface)->get_cesq(&rxlev,
                                                                   NULL,
                                                                   NULL,
                                                                   NULL,
                                                                   &rsrq,
                                                                   &rsrp);
    if (success) {
        // Convert the rxlev number to dBm
        gRssiDbm = rxLevToRssiDbm(rxlev);
        // Convert the RSRP number to dBm
        gRsrpDbm = rsrpToDbm(rsrp);
        gSnrDb = 0;
        if ((gRssiDbm < 0) && (gRsrpDbm <= gRssiDbm)) {
            // Compute the SNR
            snrDb(gRssiDbm, gRsrpDbm, &gSnrDb);
        }
        // Convert the RSRQ number to dB
        gRsrqDb = rsrqToDb(rsrq);
    }

    return success;
}

// Retrieve the data that AT+UCGED provides (SARA-R4 only)
static bool getUCGED()
{
    MBED_ASSERT(!gUseN2xxModem);

    return ((UbloxATCellularInterface *) gpInterface)->get_ucged(&gEarfcn,
                                                                 &gCellId,
                                                                 &gRsrqDb,
                                                                 &gRsrpDbm);
}

/**************************************************************************
 * PUBLIC FUNCTIONS: CELLULAR
 *************************************************************************/

// Get the received signal strengths.
ActionDriver getCellularSignalRx(int *pRsrpDbm, int *pRssiDbm,
                                 int *pRsrqDb, int *pSnrDb)
{
    ActionDriver result;
    bool success;

    MTX_LOCK(gMtx);

    result = ACTION_DRIVER_ERROR_NOT_INITIALISED;
    success = false;

    if (gpInterface != NULL) {
        // Refresh the answer if it's time, otherwise just use the
        // stored values
        result = ACTION_DRIVER_ERROR_NO_DATA;
        if (gUseN2xxModem) {
            // For SARA-N2xx everything is in NUESTATS
            success = getNUEStats();
        } else {
            // In theory we can use AT+CESQ on SARA-R4
            // however, in my experience, it tends to return
            // unknown a lot whereas AT+UCGED always returns
            // a value for RSRQ and RSRP, so leave CESQ as
            // a fall-back if UCGED is not supported
            // Don't get SNR or RSSI from AT+UCGED so zero
            // them here just in case
            gSnrDb = 0;
            gRssiDbm = 0;
            success = getUCGED() || getCESQ();
        }

        if (success) {
            if (pRsrpDbm != NULL) {
                *pRsrpDbm = gRsrpDbm;
            }
            if (pRssiDbm != NULL) {
                *pRssiDbm = gRssiDbm;
            }
            if (pRsrqDb != NULL) {
                *pRsrqDb = gRsrqDb;
            }
            if (pSnrDb != NULL) {
                *pSnrDb = gSnrDb;
            }
            result = ACTION_DRIVER_OK;
        }
    }

    MTX_UNLOCK(gMtx);

    return result;
}

// Get the transmit signal strength.
ActionDriver getCellularSignalTx(int *pPowerDbm)
{
    ActionDriver result;
    bool success;

    MTX_LOCK(gMtx);

    result = ACTION_DRIVER_ERROR_NOT_INITIALISED;
    success = false;

    if (gpInterface != NULL) {
        // Refresh the answer if it's time, otherwise just use the
        // stored values
        result = ACTION_DRIVER_ERROR_NO_DATA;
        if (gUseN2xxModem) {
            // For SARA-N2xx everything is in NUESTATS
            success = getNUEStats();
        } else {
            // Not possible to get this information from the SARA-R4xx modem
            gTxPowerDbm = 0;
        }

        if (success) {
            if (pPowerDbm != NULL) {
                *pPowerDbm = gTxPowerDbm;
            }
            result = ACTION_DRIVER_OK;
        }
    }

    MTX_UNLOCK(gMtx);

    return result;
}

// Get the channel parameters.
ActionDriver getCellularChannel(unsigned int *pCellId,
                                unsigned int *pEarfcn,
                                unsigned char *pEcl)
{
    ActionDriver result;
    bool success;

    MTX_LOCK(gMtx);

    result = ACTION_DRIVER_ERROR_NOT_INITIALISED;
    success = false;

    if (gpInterface != NULL) {
        // Refresh the answer if it's time, otherwise just use the
        // stored values
        result = ACTION_DRIVER_ERROR_NO_DATA;
        if (gUseN2xxModem) {
            // For SARA-N2xx everything is in NUESTATS
            success = getNUEStats();
        } else {
            success = getUCGED();
            // Not possible to get ECL from the SARA-R4xx modem
            gEcl = 0;
        }

        if (success) {
            if (pCellId != NULL) {
                *pCellId = (unsigned int) gCellId;
            }
            if (pEarfcn != NULL) {
                *pEarfcn = (unsigned int) gEarfcn;
            }
            if (pEcl != NULL) {
                *pEcl = (unsigned char) gEcl;
            }
            result = ACTION_DRIVER_OK;
        }
    }

    MTX_UNLOCK(gMtx);

    return result;
}

/**************************************************************************
 * PUBLIC FUNCTIONS: MODEM MANAGEMENT
 *************************************************************************/

// Initialise the modem.
ActionDriver modemInit(const char *pSimPin, const char *pApn,
                       const char *pUserName, const char *pPassword)
{
    ActionDriver result;

    MTX_LOCK(gMtx);

    result = ACTION_DRIVER_OK;

    if (gpInterface == NULL) {
        // Set the TXD and RXD pins high, a requirement for SARA-R4
        // where holding the Tx line low puts the modem to SLEEP.
        DigitalOut txd(MDMTXD, 1);
        DigitalOut rxd(MDMRXD, 1);
        // Get the CP_ON pin out of it's "wired and" mode
        pgCpOn = new DigitalOut(PIN_CP_ON, 1);

#if FORCE_N2_MODEM
        gInitialisedOnce = true;
        gUseN2xxModem = true;
#endif

        // If we've been initialised once, just instantiate the right modem
        if (gInitialisedOnce) {
            if (gUseN2xxModem) {
                gpInterface = pGetSaraN2(pSimPin, pApn, pUserName, pPassword);
            } else {
                gpInterface = pGetSaraR4(pSimPin, pApn, pUserName, pPassword);
            }
        } else {
            // Attempt to power up the R4 modem first: if the N2 modem is
            // connected instead it will not respond since it works at 9600
            // and does not auto-baud.
            gpInterface = pGetSaraR4(pSimPin, pApn, pUserName, pPassword);
            if (gpInterface == NULL) {
                // If that didn't work, try the N211 driver
                gpInterface = pGetSaraN2(pSimPin, pApn, pUserName, pPassword);
                if (gpInterface != NULL) {
                    gUseN2xxModem = true;
                }
            }
        }

        if (gpInterface != NULL) {
            gInitialisedOnce = true;
        } else {
            // Return the modem interface to its off state, since we aren't going
            // to go through the modemDeinit() procedure
            modemInterfaceOff();
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

        modemInterfaceOff();

        // Make sure the modem has time to power down
        // completely in case it is initialised again
        // immediately afterwards
        Thread::wait(2000);

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
ActionDriver modemConnect(bool (*pKeepGoingCallback)(void *),
                          void *pCallbackParam,
                          void (*pWatchdogCallback) (void))
{
    ActionDriver result;
    int x = 0;

    MTX_LOCK(gMtx);

    result = ACTION_DRIVER_ERROR_NOT_INITIALISED;

    if (gpInterface != NULL) {
        statisticsIncConnectionAttempts();
        if (gUseN2xxModem) {
            ((UbloxATCellularInterfaceN2xx *) gpInterface)->set_registration_callbacks(pKeepGoingCallback,
                                                                                       pCallbackParam,
                                                                                       pWatchdogCallback);
            x = ((UbloxATCellularInterfaceN2xx *) gpInterface)->connect();
        } else {
            ((UbloxATCellularInterface *) gpInterface)->set_registration_callbacks(pKeepGoingCallback,
                                                                                   pCallbackParam,
                                                                                   pWatchdogCallback);
            x = ((UbloxATCellularInterface *) gpInterface)->connect();
        }

        if (x == 0) {
            statisticsIncConnectionSuccess();
            result = ACTION_DRIVER_OK;
        } else {
            gLastConnectErrorCode = x;
        }
    }

    MTX_UNLOCK(gMtx);

    return result;
}

// Get the last connect error code.
int modemGetLastConnectErrorCode()
{
    return gLastConnectErrorCode;
}

// Get the time from an NTP server.
ActionDriver modemGetTime(time_t *pTimeUTC)
{
    ActionDriver result;
    UDPSocket sockUdp;
    SocketAddress udpServer;
    SocketAddress udpSenderAddress;
    Timer ackTimeout;
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
                    ackTimeout.start();
                    while ((ackTimeout.read_ms() < ACK_TIMEOUT_MS) &&
                           (result != ACTION_DRIVER_OK)) {
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
                    ackTimeout.stop();
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
                              const char *pIdString,
                              bool (pKeepGoingCallback(void *)),
                              void *pCallbackParam)
{
    ActionDriver result;
    UDPSocket sockUdp;
    SocketAddress udpServer;
    SocketAddress udpSenderAddress;
    Timer ackTimeout;
    CodecErrorOrIndex index;
    unsigned int numNeedingAck;
    unsigned int numAcked;
    int x;

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
            numNeedingAck = 0;
            numAcked = 0;
            result = ACTION_DRIVER_ERROR_OUT_OF_MEMORY;
            udpServer.set_port(serverPort);
            if (sockUdp.open(gpInterface) == 0) {
                sockUdp.set_timeout(SOCKET_TIMEOUT_MS);
                // Encode and send data until done
                result = ACTION_DRIVER_OK;
                codecPrepareData();
                // Note: need to break out of the code/send loop if ANY
                // errors occur otherwise there's a possibility that
                // codecAckData() will be called to free past data
                // that hasn't actually been acknowledged or sent
                while (((pKeepGoingCallback == NULL) ||
                        pKeepGoingCallback(pCallbackParam)) &&
                        (result == ACTION_DRIVER_OK) &&
                        (CODEC_SIZE(x = codecEncodeData(pIdString, gBuf, sizeof(gBuf),
                                                        ACK_FOR_REPORTS)) > 0)) {
                    MBED_ASSERT((CODEC_FLAGS(x) &
                                 (CODEC_FLAG_NOT_ENOUGH_ROOM_FOR_HEADER |
                                  CODEC_FLAG_NOT_ENOUGH_ROOM_FOR_EVEN_ONE_DATA)) == 0);
                    if (sockUdp.sendto(udpServer, (void *) gBuf, CODEC_SIZE(x)) == CODEC_SIZE(x)) {
                        debugPulseLed(20);
                        statisticsAddTransmitted(CODEC_SIZE(x));
                        if ((CODEC_FLAGS(x) & CODEC_FLAG_NEEDS_ACK) > 0) {
                            numNeedingAck++;
                        }
                        // Every few transmits, see if any acks have arrived
                        // so as not to buffer-overrun inside the module
                        // Note: not doing this every time as it takes a while.
                        if (numNeedingAck > numAcked) {
                            if ((numNeedingAck % 10) == 0) {
                                ackTimeout.reset();
                                ackTimeout.start();
                                while (ackTimeout.read_ms() < 2000) {
                                    if ((x = sockUdp.recvfrom(&udpSenderAddress, (void *) gAckBuf, sizeof(gAckBuf))) > 0) {
                                        statisticsAddReceived(x);
                                        index = codecDecodeAck(gAckBuf, x, pIdString);
                                        if (index >= 0) {
                                            codecAckDataIndex(index);
                                            numAcked++;
                                        }
                                    }
                                }
                                ackTimeout.stop();
                            }
                        } else {
                            // If there's nothing to ack then just wait a little
                            // between transmits instead
                            Thread::wait(100);
                        }
                    } else {
                        result = ACTION_DRIVER_ERROR_SEND_REPORTS;
                    }
                }

                // Done all the sending, wait for any acks outstanding
                ackTimeout.reset();
                ackTimeout.start();
                while ((numAcked < numNeedingAck) &&
                       (ackTimeout.read_ms() < ACK_TIMEOUT_MS)) {
                    if ((x = sockUdp.recvfrom(&udpSenderAddress, (void *) gAckBuf, sizeof(gAckBuf))) > 0) {
                        statisticsAddReceived(x);
                        index = codecDecodeAck(gAckBuf, x, pIdString);
                        if (index >= 0) {
                            codecAckDataIndex(index);
                            numAcked++;
                        }
                    }
                }
                ackTimeout.stop();

                sockUdp.close();
            }
        }
    }

    MTX_UNLOCK(gMtx);

    return result;
}

// Determine the type of modem attached.
bool modemIsN2()
{
    return gUseN2xxModem;
}

// Determine the type of modem attached.
bool modemIsR4()
{
    return !gUseN2xxModem;
}

// Determine the energy consumed by the modem.
unsigned long long int modemEnergyNWH(unsigned int idleTimeSeconds,
                                      unsigned int bytesTransmitted)
{
    unsigned long long int energyNWH = 0;

    if (gUseN2xxModem) {
        if (idleTimeSeconds > 0) {
            energyNWH += ((unsigned long long int) idleTimeSeconds) * CELLULAR_N2XX_POWER_IDLE_NW / 3600;
        } else {
            energyNWH += CELLULAR_N2XX_POWER_REGISTRATION_NWH;
        }
        energyNWH += CELLULAR_N2XX_ENERGY_TX_NWH(bytesTransmitted);
    } else {
        if (idleTimeSeconds > 0) {
            energyNWH += ((unsigned long long int) idleTimeSeconds) * CELLULAR_R410_POWER_IDLE_NW / 3600;
        } else {
            energyNWH += CELLULAR_R410_POWER_REGISTRATION_NWH;
        }
        energyNWH += CELLULAR_R410_ENERGY_TX_NWH(bytesTransmitted);
    }

    return energyNWH;
}

// End of file
