/* Copyright (c) 2017 ublox Limited
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

#include "UARTSerial.h"
#include "APN_db.h"
#include "UbloxCellularBase.h"
#include "onboard_modem_api.h"
#ifdef FEATURE_COMMON_PAL
#include "mbed_trace.h"
#define TRACE_GROUP "UCB"
#else
#define tr_debug(format, ...) debug_if(_debug_trace_on, format "\n", ## __VA_ARGS__)
#define tr_info(format, ...)  debug_if(_debug_trace_on, format "\n", ## __VA_ARGS__)
#define tr_warn(format, ...)  debug_if(_debug_trace_on, format "\n", ## __VA_ARGS__)
#define tr_error(format, ...) debug_if(_debug_trace_on, format "\n", ## __VA_ARGS__)
#endif

/* Array to convert the 3G qual number into a median EC_NO_LEV number.
 */
                            /* 0   1   2   3   4   5   6  7 */
const int qualConvert3G[] = {44, 41, 35, 29, 23, 17, 11, 7};

/* Array to convert the 3G "rssi" number into a dBm RSCP value rounded up to the
 * nearest whole number.
 */
const int rscpConvert3G[] = {-108, -105, -103, -100,  -98,  -96,  -94,  -93,   /* 0 - 7 */
                              -91,  -89,  -88,  -85,  -83,  -80,  -78,  -76,   /* 8 - 15 */
                              -74,  -73,  -70,  -68,  -66,  -64,  -63,  -60,   /* 16 - 23 */
                              -58,  -56,  -54,  -53,  -51,  -49,  -48,  -46};  /* 24 - 31 */

/* Array to convert the LTE rssi number into a dBm value rounded up to the
 * nearest whole number.
 */
const int rssiConvertLte[] = {-118, -115, -113, -110, -108, -105, -103, -100,   /* 0 - 7 */
                               -98,  -95,  -93,  -90,  -88,  -85,  -83,  -80,   /* 8 - 15 */
                               -78,  -76,  -74,  -73,  -71,  -69,  -68,  -65,   /* 16 - 23 */
                               -63,  -61,  -60,  -59,  -58,  -55,  -53,  -48};  /* 24 - 31 */

/**********************************************************************
 * PRIVATE METHODS
 **********************************************************************/

/** A simple implementation of atoi() for positive, perfectly
 * formed numbers.  Needed in order to avoid using atoi() as
 * that requires some obscure RTX configuration to do with
 * OS_THREAD_LIBSPACE_NUM.
 */
int UbloxCellularBase::ascii_to_int(const char *buf)
{
    unsigned int answer = 0;

     for (int x = 0; *buf != 0; x++) {
         answer = answer * 10 + *buf - '0';
         buf++;
     }

     return (int) answer;
}

void UbloxCellularBase::set_nwk_reg_status_csd(int status)
{
    switch (status) {
        case CSD_NOT_REGISTERED_NOT_SEARCHING:
        case CSD_NOT_REGISTERED_SEARCHING:
            tr_info("Not (yet) registered for circuit switched service");
            break;
        case CSD_REGISTERED:
        case CSD_REGISTERED_ROAMING:
            tr_info("Registered for circuit switched service");
            break;
        case CSD_REGISTRATION_DENIED:
            tr_info("Circuit switched service denied");
            break;
        case CSD_UNKNOWN_COVERAGE:
            tr_info("Out of circuit switched service coverage");
            break;
        case CSD_SMS_ONLY:
            tr_info("SMS service only");
            break;
        case CSD_SMS_ONLY_ROAMING:
            tr_info("SMS service only");
            break;
        case CSD_CSFB_NOT_PREFERRED:
            tr_info("Registered for circuit switched service with CSFB not preferred");
            break;
        default:
            tr_info("Unknown circuit switched service registration status. %d", status);
            break;
    }

    _dev_info.reg_status_csd = static_cast<NetworkRegistrationStatusCsd>(status);
}

void UbloxCellularBase::set_nwk_reg_status_psd(int status)
{
    switch (status) {
        case PSD_NOT_REGISTERED_NOT_SEARCHING:
        case PSD_NOT_REGISTERED_SEARCHING:
            tr_info("Not (yet) registered for packet switched service");
            break;
        case PSD_REGISTERED:
        case PSD_REGISTERED_ROAMING:
            tr_info("Registered for packet switched service");
            break;
        case PSD_REGISTRATION_DENIED:
            tr_info("Packet switched service denied");
            break;
        case PSD_UNKNOWN_COVERAGE:
            tr_info("Out of packet switched service coverage");
            break;
        case PSD_EMERGENCY_SERVICES_ONLY:
            tr_info("Limited access for packet switched service. Emergency use only.");
            break;
        default:
            tr_info("Unknown packet switched service registration status. %d", status);
            break;
    }

    _dev_info.reg_status_psd = static_cast<NetworkRegistrationStatusPsd>(status);
}

void UbloxCellularBase::set_nwk_reg_status_eps(int status)
{
    switch (status) {
        case EPS_NOT_REGISTERED_NOT_SEARCHING:
        case EPS_NOT_REGISTERED_SEARCHING:
            tr_info("Not (yet) registered for EPS service");
            break;
        case EPS_REGISTERED:
        case EPS_REGISTERED_ROAMING:
            tr_info("Registered for EPS service");
            break;
        case EPS_REGISTRATION_DENIED:
            tr_info("EPS service denied");
            break;
        case EPS_UNKNOWN_COVERAGE:
            tr_info("Out of EPS service coverage");
            break;
        case EPS_EMERGENCY_SERVICES_ONLY:
            tr_info("Limited access for EPS service. Emergency use only.");
            break;
        default:
            tr_info("Unknown EPS service registration status. %d", status);
            break;
    }

    _dev_info.reg_status_eps = static_cast<NetworkRegistrationStatusEps>(status);
}

void UbloxCellularBase::rat(int acTStatus)
{
    switch (acTStatus) {
        case GSM:
        case COMPACT_GSM:
            tr_info("Connected in GSM");
            break;
        case UTRAN:
            tr_info("Connected to UTRAN");
            break;
        case EDGE:
            tr_info("Connected to EDGE");
            break;
        case HSDPA:
            tr_info("Connected to HSDPA");
            break;
        case HSUPA:
            tr_info("Connected to HSPA");
            break;
        case HSDPA_HSUPA:
            tr_info("Connected to HDPA/HSPA");
            break;
        case LTE:
            tr_info("Connected to LTE");
            break;
        case EC_GSM_IoT:
            tr_info("Connected to EC_GSM_IoT");
            break;
        case E_UTRAN_NB_S1:
            tr_info("Connected to E_UTRAN NB1");
            break;
        default:
            tr_info("Unknown RAT %d", acTStatus);
            break;
    }

    _dev_info.rat = static_cast<RadioAccessNetworkType>(acTStatus);
}

bool UbloxCellularBase::get_iccid()
{
    bool success;
    LOCK();

    MBED_ASSERT(_at != NULL);

    // Returns the ICCID (Integrated Circuit Card ID) of the SIM-card.
    // ICCID is a serial number identifying the SIM.
    // AT Command Manual UBX-13002752, section 4.12
    success = _at->send("AT+CCID") && _at->recv("+CCID: %20[^\n]\nOK\n", _dev_info.iccid);
    tr_info("DevInfo: ICCID=%s", _dev_info.iccid);

    UNLOCK();
    return success;
}

bool UbloxCellularBase::get_imsi()
{
    bool success;
    LOCK();

    MBED_ASSERT(_at != NULL);

    // International mobile subscriber identification
    // AT Command Manual UBX-13002752, section 4.11
    success = _at->send("AT+CIMI") && _at->recv("%15[^\n]\nOK\n", _dev_info.imsi);
    tr_info("DevInfo: IMSI=%s", _dev_info.imsi);

    UNLOCK();
    return success;
}

bool UbloxCellularBase::get_imei()
{
    bool success;
    LOCK();

    MBED_ASSERT(_at != NULL);

    // International mobile equipment identifier
    // AT Command Manual UBX-13002752, section 4.7
    success = _at->send("AT+CGSN") && _at->recv("%15[^\n]\nOK\n", _dev_info.imei);
    tr_info("DevInfo: IMEI=%s", _dev_info.imei);

    UNLOCK();
    return success;
}

bool UbloxCellularBase::get_meid()
{
    bool success;
    LOCK();

    MBED_ASSERT(_at != NULL);

    // Mobile equipment identifier
    // AT Command Manual UBX-13002752, section 4.8
    success = _at->send("AT+GSN") && _at->recv("%18[^\n]\nOK\n", _dev_info.meid);
    tr_info("DevInfo: MEID=%s", _dev_info.meid);

    UNLOCK();
    return success;
}

bool UbloxCellularBase::set_sms()
{
    bool success = false;
    char buf[32];
    LOCK();

    MBED_ASSERT(_at != NULL);

    // Set up SMS format and enable URC
    // AT Command Manual UBX-13002752, section 11
    if (_at->send("AT+CMGF=1") && _at->recv("OK")) {
        tr_debug("SMS in text mode");
        if (_at->send("AT+CNMI=2,1") && _at->recv("OK")) {
            tr_debug("SMS URC enabled");
            // Set to CS preferred since PS preferred doesn't work
            // on some networks
            if (_at->send("AT+CGSMS=1") && _at->recv("OK")) {
                tr_debug("SMS set to CS preferred");
                success = true;
                memset (buf, 0, sizeof (buf));
                if (_at->send("AT+CSCA?") &&
                    _at->recv("+CSCA: \"%31[^\"]\"", buf) &&
                    _at->recv("OK")) {
                    tr_info("SMS Service Centre address is \"%s\"", buf);
                }
            }
        }
    }

    UNLOCK();
    return success;
}

void UbloxCellularBase::parser_abort_cb()
{
    _at->abort();
}

// Callback for CME ERROR and CMS ERROR.
void UbloxCellularBase::CMX_ERROR_URC()
{
    char buf[48];

    if (read_at_to_char(buf, sizeof (buf), '\r') > 0) {
        tr_debug("AT error %s", buf);
        // If it's a number and there's a CME Error callback, call it
        if ((buf[0] >= '0') && (buf[0] <= '9') && _cme_error_callback) {
            _cme_error_callback(ascii_to_int(buf));
        }
    }
    parser_abort_cb();
}

// Callback for circuit switched registration URC.
void UbloxCellularBase::CREG_URC()
{
    char buf[10];
    int status;
    int acTStatus;

    // If this is the URC it will be a single
    // digit followed by \n.  If it is the
    // answer to a CREG query, it will be
    // a ": %d,%d\n" where the second digit
    // indicates the status
    // Note: not calling _at->recv() from here as we're
    // already in an _at->recv()
    if (read_at_to_char(buf, sizeof (buf), '\n') > 0) {
        if (sscanf(buf, ": %*d,%d,%*d,%*d,%d,", &status, &acTStatus) == 2) {
            set_nwk_reg_status_csd(status);
            rat(acTStatus);
        } else if (sscanf(buf, ": %*d,%d", &status) == 1) {
            set_nwk_reg_status_csd(status);
        } else if (sscanf(buf, ": %d", &status) == 1) {
            set_nwk_reg_status_csd(status);
        }
    }
}

// Callback for packet switched registration URC.
void UbloxCellularBase::CGREG_URC()
{
    char buf[10];
    int status;
    int acTStatus;

    // If this is the URC it will be a single
    // digit followed by \n.  If it is the
    // answer to a CGREG query, it will be
    // a ": %d,%d\n" where the second digit
    // indicates the status
    // Note: not calling _at->recv() from here as we're
    // already in an _at->recv()
    if (read_at_to_char(buf, sizeof (buf), '\n') > 0) {
        if (sscanf(buf, ": %*d,%d,%*d,%*d,%d,", &status, &acTStatus) == 2) {
            set_nwk_reg_status_csd(status);
            rat(acTStatus);
        } else if (sscanf(buf, ": %*d,%d", &status) == 1) {
            set_nwk_reg_status_psd(status);
        } else if (sscanf(buf, ": %d", &status) == 1) {
            set_nwk_reg_status_psd(status);
        }
    }
}

// Callback for EPS registration URC.
void UbloxCellularBase::CEREG_URC()
{
    char buf[50];
    int status;
    int acTStatus;

    // If this is the URC it will be a single
    // digit followed by \n.  If it is the
    // answer to a CEREG query, it will be
    // a ": %d,%d\n" where the second digit
    // indicates the status
    // Note: not calling _at->recv() from here as we're
    // already in an _at->recv()
    if (read_at_to_char(buf, sizeof (buf), '\n') > 0) {
        tr_debug("+CEREG%s\n", buf);
        if (sscanf(buf, ": %*d,%d,%*d,%*d,%d,", &status, &acTStatus) == 2) {
            set_nwk_reg_status_csd(status);
            rat(acTStatus);
        } else if (sscanf(buf, ": %*d,%d", &status) == 1) {
            set_nwk_reg_status_eps(status);
        } else if (sscanf(buf, ": %d", &status) == 1) {
            set_nwk_reg_status_eps(status);
        }
    }
}

// Callback UMWI, just filtering it out.
void UbloxCellularBase::UMWI_URC()
{
    char buf[10];

    // Note: not calling _at->recv() from here as we're
    // already in an _at->recv()
    read_at_to_char(buf, sizeof (buf), '\n');
}

/**********************************************************************
 * PROTECTED METHODS
 **********************************************************************/

#if MODEM_ON_BOARD
void UbloxCellularBase::modem_init()
{
    ::onboard_modem_init();
}

void UbloxCellularBase::modem_deinit()
{
    ::onboard_modem_deinit();
}

void UbloxCellularBase::modem_power_up()
{
    ::onboard_modem_power_up();
}

void UbloxCellularBase::modem_power_down()
{
    ::onboard_modem_power_down();
}
#else
void UbloxCellularBase::modem_init()
{
    // Meant to be overridden
}

void UbloxCellularBase::modem_deinit()
{
    // Meant to be overridden
}

void UbloxCellularBase::modem_power_up()
{
    // Meant to be overridden
}

void UbloxCellularBase::modem_power_down()
{
    // Mmeant to be overridden
}
#endif

// Constructor.
// Note: to allow this base class to be inherited as a virtual base class
// by everyone, it takes no parameters.  See also comment above classInit()
// in the header file.
UbloxCellularBase::UbloxCellularBase()
{
    _pin = NULL;
    _at = NULL;
    _at_timeout = AT_PARSER_TIMEOUT;
    _fh = NULL;
    _modem_initialised = false;
    _baud = 9600;
    _sim_pin_check_enabled = false;
    _debug_trace_on = false;
    _cscon_callback = NULL;
    _cme_error_callback = NULL;
    _rat = -1;
    _band_mask = 0;

    _dev_info.dev = DEV_TYPE_NONE;
    _dev_info.reg_status_csd = CSD_NOT_REGISTERED_NOT_SEARCHING;
    _dev_info.reg_status_psd = PSD_NOT_REGISTERED_NOT_SEARCHING;
    _dev_info.reg_status_eps = EPS_NOT_REGISTERED_NOT_SEARCHING;
}

// Destructor.
UbloxCellularBase::~UbloxCellularBase()
{
    delete _at;
    delete _fh;
}

// Initialise the portions of this class that are parameterised.
void UbloxCellularBase::baseClassInit(PinName tx, PinName rx,
                                      int baud, bool debug_on)
{
    // Only initialise ourselves if it's not already been done
    if (_at == NULL) {
        if (_debug_trace_on == false) {
            _debug_trace_on = debug_on;
        }
        _baud = baud;

        // Set up File Handle for buffered serial comms with cellular module
        // (which will be used by the AT parser)
        // Note: the UART is initialised to run no faster than 115200 because
        // the modems cannot reliably auto-baud at faster rates.  The faster
        // rate is adopted later with a specific AT command and the
        // UARTSerial rate is adjusted at that time
        if (baud > 115200) {
            baud = 115200;
        }
        _fh = new UARTSerial(tx,  rx, baud);

        // Set up the AT parser
#ifndef MODEM_IS_2G_3G
        _at = new UbloxATCmdParser(_fh, OUTPUT_ENTER_KEY, AT_PARSER_BUFFER_SIZE,
                                   _at_timeout, _debug_trace_on);
#else
        _at = new ATCmdParser(_fh, OUTPUT_ENTER_KEY, AT_PARSER_BUFFER_SIZE,
                              _at_timeout, _debug_trace_on);
#endif

        // Error cases, out of band handling
        _at->oob("ERROR", callback(this, &UbloxCellularBase::parser_abort_cb));
        // Deliberately include the colon below to make parsing of the remainder easier
        _at->oob("+CME ERROR:", callback(this, &UbloxCellularBase::CMX_ERROR_URC));
        _at->oob("+CMS ERROR:", callback(this, &UbloxCellularBase::CMX_ERROR_URC));

        // Registration status, out of band handling
        _at->oob("+CREG", callback(this, &UbloxCellularBase::CREG_URC));
        _at->oob("+CGREG", callback(this, &UbloxCellularBase::CGREG_URC));
        _at->oob("+CEREG", callback(this, &UbloxCellularBase::CEREG_URC));

        // Capture the UMWI, just to stop it getting in the way
        _at->oob("+UMWI", callback(this, &UbloxCellularBase::UMWI_URC));
    }
}

// Set the AT parser timeout.
// Note: the AT interface should be locked before this is called.
void UbloxCellularBase::at_set_timeout(int timeout) {

    MBED_ASSERT(_at != NULL);

    _at_timeout = timeout;
    _at->set_timeout(timeout);
}

// Read up to size bytes from the AT interface up to a "end".
// Note: the AT interface should be locked before this is called.
int UbloxCellularBase::read_at_to_char(char * buf, int size, char end)
{
    int count = 0;
    int x = 0;

    if (size > 0) {
        for (count = 0; (count < size) && (x >= 0) && (x != end); count++) {
            x = _at->getc();
            *(buf + count) = (char) x;
        }

        count--;
        *(buf + count) = 0;

        // Convert line endings:
        // If end was '\n' (0x0a) and the preceding character was 0x0d, then
        // overwrite that with null as well.
        if ((count > 0) && (end == '\n') && (*(buf + count - 1) == '\x0d')) {
            count--;
            *(buf + count) = 0;
        }
    }

    return count;
}

// Power up the modem.
// Enables the GPIO lines to the modem and then wriggles the power line in short pulses.
bool UbloxCellularBase::power_up()
{
    bool success = false;
    int at_timeout;
    LOCK();

    at_timeout = _at_timeout; // Has to be inside LOCK()s

    MBED_ASSERT(_at != NULL);

    /* Initialize GPIO lines */
    tr_info("Powering up non-N2xx modem...");
    modem_init();
    /* Give modem a little time to settle down */
    Thread::wait(250);

#ifdef MODEM_IS_2G_3G
    // U201 needs around 13 prods
    for (int retry_count = 0; !success && (retry_count < 20); retry_count++) {
#else
    for (int retry_count = 0; !success && (retry_count < 10); retry_count++) {
#endif
        //In case of SARA-R4, modem takes a while to turn on, constantly toggling the power pin every ~2 secs causes the modem to never power up.
        if ( (retry_count % 5) == 0) {
            modem_power_up();
        }
        Thread::wait(500);
        // Modem tends to spit out noise during power up - don't confuse the parser
        _at->flush();
        at_set_timeout(1000);
        if (_at->send("AT")) {
            // C027 needs a delay here
            Thread::wait(100);
            if (_at->recv("OK")) {
                success = true;
            }
        }
        at_set_timeout(at_timeout);
    }

    if (success) {
        // Set the final baud rate
        if (_at->send("AT+IPR=%d", _baud) && _at->recv("OK")) {
            // Need to wait for things to be sorted out on the modem side
            Thread::wait(100);
            ((UARTSerial *)_fh)->set_baud(_baud);
        }

        // Turn off modem echoing and turn on verbose responses
        success = _at->send("ATE0;+CMEE=2") && _at->recv("OK") &&
                  // The following commands are best sent separately
                  _at->send("AT&K0") && _at->recv("OK") && // Turn off RTC/CTS handshaking
                  _at->send("AT&C1") && _at->recv("OK") && // Set DCD circuit(109), changes in accordance with the carrier detect status
                  _at->send("AT&D0") && _at->recv("OK"); // Set DTR circuit, we ignore the state change of DTR

        // Switch on channel and environment reporting (work on SARA-R4 only so
        // don't care if it fails)
        _at->send("AT+UCGED=5") && _at->recv("OK");
    }

    if (!success) {
        tr_error("Preliminary modem setup failed.");
    }

    UNLOCK();
    return success;
}

// Power down modem via AT interface.
void UbloxCellularBase::power_down()
{
    LOCK();

    MBED_ASSERT(_at != NULL);

    // If we have been running, do a soft power-off first
    if (_modem_initialised && (_at != NULL)) {
        _at->send("AT+CPWROFF") && _at->recv("OK");
    }

    // Now do a hard power-off
    modem_power_down();
    modem_deinit();


    _dev_info.reg_status_csd = CSD_NOT_REGISTERED_NOT_SEARCHING;
    _dev_info.reg_status_psd = PSD_NOT_REGISTERED_NOT_SEARCHING;
    _dev_info.reg_status_eps = EPS_NOT_REGISTERED_NOT_SEARCHING;

   UNLOCK();
}

// Get the device ID.
bool UbloxCellularBase::set_device_identity(DeviceType *dev)
{
    char buf[20];
    bool success;
    LOCK();

    MBED_ASSERT(_at != NULL);

    success = _at->send("ATI") && _at->recv("%19[^\n]\nOK\n", buf);

    if (success) {
        if (strstr(buf, "SARA-G35"))
            *dev = DEV_SARA_G35;
        else if (strstr(buf, "LISA-U200-03S"))
            *dev = DEV_LISA_U2_03S;
        else if (strstr(buf, "LISA-U2"))
            *dev = DEV_LISA_U2;
        else if (strstr(buf, "SARA-U2"))
            *dev = DEV_SARA_U2;
        else if (strstr(buf, "SARA-R4"))
            *dev = DEV_SARA_R4;
        else if (strstr(buf, "LEON-G2"))
            *dev = DEV_LEON_G2;
        else if (strstr(buf, "TOBY-L2"))
            *dev = DEV_TOBY_L2;
        else if (strstr(buf, "MPCI-L2"))
            *dev = DEV_MPCI_L2;
    }

    UNLOCK();
    return success;
}

// Send initialisation AT commands that are specific to the device.
bool UbloxCellularBase::device_init(DeviceType dev)
{
    bool success = false;
    LOCK();

    MBED_ASSERT(_at != NULL);

    if ((dev == DEV_LISA_U2) || (dev == DEV_LEON_G2) || (dev == DEV_TOBY_L2)) {
        success = _at->send("AT+UGPIOC=20,2") && _at->recv("OK");
    } else if ((dev == DEV_SARA_U2) || (dev == DEV_SARA_G35)) {
        success = _at->send("AT+UGPIOC=16,2") && _at->recv("OK");
    } else {
        success = true;
    }

    UNLOCK();
    return success;
}

// Get the SIM card going.
bool UbloxCellularBase::initialise_sim_card()
{
    bool success = false;
    int retry_count = 0;
    bool done = false;
    LOCK();

    MBED_ASSERT(_at != NULL);

    /* SIM initialisation may take a significant amount, so an error is
     * kind of expected. We should retry 10 times until we succeed or timeout. */
    for (retry_count = 0; !done && (retry_count < 10); retry_count++) {
        char pinstr[16];

        if (_at->send("AT+CPIN?") && _at->recv("+CPIN: %15[^\n]\n", pinstr) &&
            _at->recv("OK")) {
            done = true;
            if (strcmp(pinstr, "SIM PIN") == 0) {
                _sim_pin_check_enabled = true;
                if (_at->send("AT+CPIN=\"%s\"", _pin)) {
                    if (_at->recv("OK")) {
                        tr_info("PIN correct");
                        success = true;
                    } else {
                        tr_error("Incorrect PIN");
                    }
                }
            } else if (strcmp(pinstr, "READY") == 0) {
                _sim_pin_check_enabled = false;
                tr_info("No PIN required");
                success = true;
            } else {
                tr_debug("Unexpected response from SIM: \"%s\"", pinstr);
            }
        }

        /* wait for a second before retry */
        Thread::wait(1000);
    }

    if (done) {
        tr_info("SIM Ready.");
    } else {
        tr_error("SIM not ready.");
    }

    UNLOCK();
    return success;
}

// Perform the pre-initialisation steps: power up and check
// stored configuration of various things.
bool UbloxCellularBase::pre_init(int mno_profile,
                                 int rat,
                                 unsigned long long int band_mask)
{
    int count = 0;
    bool success = false;
#ifndef MODEM_IS_2G_3G
    bool mno_profile_correct = false;
    bool rat_correct = false;
    bool band_mask_correct = false;
#endif

    MBED_ASSERT(_at != NULL);

    // Note: if you change the count limit here
    // then change the else condition also
    while (!success && (count < 4)) {
        if (power_up()) {
            tr_info("Modem Ready.");
#ifdef MODEM_IS_2G_3G
            success = true;
#else
            // Check the MNO Profile
            tr_debug("Wanted mno_profile %d", mno_profile);
            if (mno_profile != get_mno_profile()) {
                set_mno_profile(mno_profile);
                set_modem_reboot();
                count++;
            } else {
                mno_profile_correct = true;
                // Check the RAT
                if (rat >= 0) {
                    tr_debug("Wanted rat %d", rat);
                    if (rat != get_rat(0)) {
                        set_rat(rat);
                        set_modem_reboot();
                        count++;
                    } else {
                        rat_correct = true;
                        tr_debug("Wanted band_mask %llu", band_mask);
                        // Check the band mask
                        if (band_mask != get_band_mask(rat)) {
                            set_band_mask(rat, band_mask);
                            set_modem_reboot();
                            count++;
                        } else {
                            band_mask_correct = true;
                        }
                    }
                } else {
                    // For diagnostics
                    get_band_mask(get_rat(0));
                    rat_correct = true;
                    band_mask_correct = true;
                }
            }

            success = mno_profile_correct &&
                      rat_correct &&
                      band_mask_correct;
#endif
        } else {
            // Fail straight away, no reason to waste power
            count = 4;
        }
    }

    return success;
}

/**********************************************************************
 * PUBLIC METHODS
 **********************************************************************/

// Initialise the modem.
bool UbloxCellularBase::init(const char *pin)
{
	int x = 0;
    MBED_ASSERT(_at != NULL);

#ifndef MODEM_IS_2G_3G
    if (!_modem_initialised || _at->get_psm_status()) {
#else
    if (!_modem_initialised) {
#endif
        if (pre_init(0, _rat, _band_mask)) {
            if (pin != NULL) {
                _pin = pin;
            }
            if (initialise_sim_card()) {
                if (set_device_identity(&_dev_info.dev) && // Set up device identity
                    device_init(_dev_info.dev)) {// Initialise this device
                    // Get the integrated circuit ID of the SIM
                    if (get_iccid()) {
                        // Try a few times to get the IMSI (since on some modems this can
                        // take a while to be retrieved, especially if a SIM PIN
                        // was set)
                        for (x = 0; (x < 3) && !get_imsi(); x++) {
                            Thread::wait(1000);
                        }
                        
                        if (x < 3) { // If we got the IMSI, can get the others
                            // When getting the IMEI I've seen occasional character loss so,
                            // since this is a pretty critical number, check it and try again
                            // if it's not 15 digits long
                            for (x = 0; (x < 3) && (!get_imei() ||
                                                    (strlen(_dev_info.imei) < 15)); x++) {
                                Thread::wait(1000);
                            }
                            if (x < 3) { // If we got the IMSI, can get the others
                                if (get_meid() /* && // Probably the same as the IMEI
                                    set_sms() */) {  // Don't set up SMS as this can fail if the
                                                     // SIM is not ready and we don't need it anyway
                                    // The modem is initialised.
                                    _modem_initialised = true;
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    return _modem_initialised;
}

// Perform registration.
bool UbloxCellularBase::nwk_registration(bool (*keep_going_callback) (void *),
                                         void *callback_param,
                                         void (*watchdog_callback) (void))
{
    bool atSuccess = false;
    bool registered = false;
    int status;
    bool modem_is_alive = true;
    int at_timeout;
    LOCK();

    at_timeout = _at_timeout; // Has to be inside LOCK()s

    MBED_ASSERT(_at != NULL);

    if (!is_registered_psd() && !is_registered_csd() && !is_registered_eps()) {
        tr_info("Searching Network...");
        // Enable the packet switched and network registration unsolicited result codes
        if (_at->send("AT+CREG=1") && _at->recv("OK") &&
            _at->send("AT+CGREG=1") && _at->recv("OK")) {
            atSuccess = true;
            if (_at->send("AT+CEREG=1")) {
                _at->recv("OK");
                // Don't check return value as this works for LTE only
            }

            if (atSuccess) {
                // See if we are already in automatic mode
                if (_at->send("AT+COPS?") && _at->recv("+COPS: %d", &status) &&
                    _at->recv("OK")) {
                    // If not, set it
                    if (status != 0) {
                        // Don't check return code here as there's not much
                        // we can do if this fails.
                        _at->send("AT+COPS=0") && _at->recv("OK");
                    }
                }

                // Query the registration status directly as well,
                // just in case
                if (_at->send("AT+CREG?") && _at->recv("OK")) {
                    // Answer will be processed by URC
                }
                if (_at->send("AT+CGREG?") && _at->recv("OK")) {
                    // Answer will be processed by URC
                }
                if (_at->send("AT+CEREG?")) {
                    _at->recv("OK");
                    // Don't check return value as this works for LTE only
                }
            }
        }
        // Wait for registration to succeed
        at_set_timeout(1000);
        while (!registered && modem_is_alive &&
               ((keep_going_callback == NULL) || keep_going_callback(callback_param))) {
            registered = is_registered_psd() || is_registered_csd() || is_registered_eps();
            // Rather than doing the "UNNATURAL_STRING" thing,
            // do a real query of CEREG.  This performs the same
            // function while, at the same time, checking CEREG and
            // being a check that the modem is really still there.
#ifdef MODEM_IS_2G_3G
            modem_is_alive = _at->send("AT+CREG?") && _at->recv("OK");
#else
            modem_is_alive = _at->send("AT+CEREG?") && _at->recv("OK");
#endif
            if (watchdog_callback) {
                watchdog_callback();
            }
            Thread::wait(1000);
        }
        at_set_timeout(at_timeout);

        if (registered) {
            // This should return quickly but sometimes the status field is not returned
            // so make the timeout short
            at_set_timeout(1000);
            if (_at->send("AT+COPS?") && _at->recv("+COPS: %*d,%*d,\"%*[^\"]\",%d\nOK\n", &status)) {
                rat(status);
            }
            at_set_timeout(at_timeout);
#ifndef MODEM_IS_2G_3G
            // For diagnostics
            _at->send("AT+CPSMS?") && _at->recv("OK");
#endif
        }
    } else {
        registered = true;
    }
    
    UNLOCK();
    return registered;
}

bool UbloxCellularBase::is_registered_csd()
{
  return (_dev_info.reg_status_csd == CSD_REGISTERED) ||
          (_dev_info.reg_status_csd == CSD_REGISTERED_ROAMING) ||
          (_dev_info.reg_status_csd == CSD_CSFB_NOT_PREFERRED);
}

bool UbloxCellularBase::is_registered_psd()
{
    return (_dev_info.reg_status_psd == PSD_REGISTERED) ||
            (_dev_info.reg_status_psd == PSD_REGISTERED_ROAMING);
}

bool UbloxCellularBase::is_registered_eps()
{
    return (_dev_info.reg_status_eps == EPS_REGISTERED) ||
            (_dev_info.reg_status_eps == EPS_REGISTERED_ROAMING);
}

// Perform deregistration.
bool UbloxCellularBase::nwk_deregistration()
{
    bool success = false;
    LOCK();

    MBED_ASSERT(_at != NULL);

    if (_at->send("AT+COPS=2") && _at->recv("OK")) {
        _dev_info.reg_status_csd = CSD_NOT_REGISTERED_NOT_SEARCHING;
        _dev_info.reg_status_psd = PSD_NOT_REGISTERED_NOT_SEARCHING;
        _dev_info.reg_status_eps = EPS_NOT_REGISTERED_NOT_SEARCHING;
        success = true;
    }

    UNLOCK();
    return success;
}

// Put the modem into its lowest power state.
void UbloxCellularBase::deinit()
{
    power_down();
    _modem_initialised = false;
}

// Set the PIN.
void UbloxCellularBase::set_pin(const char *pin) {
    _pin = pin;
}

// Enable or disable SIM pin checking.
bool UbloxCellularBase::sim_pin_check_enable(bool enable_not_disable)
{
    bool success = false;;
    LOCK();

    MBED_ASSERT(_at != NULL);

    if (_pin != NULL) {
        if (_sim_pin_check_enabled && !enable_not_disable) {
            // Disable the SIM lock
            if (_at->send("AT+CLCK=\"SC\",0,\"%s\"", _pin) && _at->recv("OK")) {
                _sim_pin_check_enabled = false;
                success = true;
            }
        } else if (!_sim_pin_check_enabled && enable_not_disable) {
            // Enable the SIM lock
            if (_at->send("AT+CLCK=\"SC\",1,\"%s\"", _pin) && _at->recv("OK")) {
                _sim_pin_check_enabled = true;
                success = true;
            }
        } else {
            success = true;
        }
    }

    UNLOCK();
    return success;
}

// Change the pin code for the SIM card.
bool UbloxCellularBase::change_sim_pin(const char *pin)
{
    bool success = false;;
    LOCK();

    MBED_ASSERT(_at != NULL);

    // Change the SIM pin
    if ((pin != NULL) && (_pin != NULL)) {
        if (_at->send("AT+CPWD=\"SC\",\"%s\",\"%s\"", _pin, pin) && _at->recv("OK")) {
            _pin = pin;
            success = true;
        }
    }

    UNLOCK();
    return success;
}

// Get the IMEI of the module.
const char *UbloxCellularBase::imei()
{
    return _dev_info.imei;
}

// Get the Mobile Equipment ID (which may be the same as the IMEI).
const char *UbloxCellularBase::meid()
{
    return _dev_info.meid;
}

// Get the IMSI of the SIM.
const char *UbloxCellularBase::imsi()
{
    // (try) to update the IMSI, just in case the SIM has changed
    get_imsi();
    
    return _dev_info.imsi;
}

// Get the ICCID of the SIM.
const char *UbloxCellularBase::iccid()
{
    // (try) to update the ICCID, just in case the SIM has changed
    get_iccid();
    
    return _dev_info.iccid;
}

// Get the RSSI in dBm.
int UbloxCellularBase::rssi()
{
    char buf[7] = {0};
    int rssi = 0;
    int qual = 0;
    int rssiRet = 0;
    bool success;
    LOCK();

    MBED_ASSERT(_at != NULL);

    success = _at->send("AT+CSQ") && _at->recv("+CSQ: %6[^\n]\nOK\n", buf);

    if (success) {
        if (sscanf(buf, "%d,%d", &rssi, &qual) == 2) {
            // AT+CSQ returns a coded RSSI value and an RxQual value
            // For 2G an RSSI of 0 corresponds to -113 dBm or less, 
            // an RSSI of 31 corresponds to -51 dBm or less and hence
            // each value is a 2 dB step.
            // For LTE the mapping is defined in the array rssiConvertLte[].
            // For 3G the mapping to RSCP is defined in the array rscpConvert3G[]
            // and the RSSI value is then RSCP - the EC_NO_LEV number derived
            // by putting the qual number through qualConvert3G[].
            if ((rssi >= 0) && (rssi <= 31)) {
                switch (_dev_info.rat) {
                    case UTRAN:
                    case HSDPA:
                    case HSUPA:
                    case HSDPA_HSUPA:
                        // 3G
                        if ((qual >= 0) && (qual <= 7)) {
                            qual = qualConvert3G[qual];
                        }
                        rssiRet = rscpConvert3G[rssi];
                        rssiRet -= qual;
                        break;
                    case LTE:
                        // LTE
                        rssiRet = rssiConvertLte[rssi];
                        break;
                    case GSM:
                    case COMPACT_GSM:
                    case EDGE:
                    default:
                        // GSM or assumed GSM if the RAT is not known
                        rssiRet = -(113 - (rssi << 2));
                        break;
                }
            }
        }
    }

    UNLOCK();
    return rssiRet;
}

// Get the contents of AT+CESQ.
bool UbloxCellularBase::get_cesq(int *rxlev, int *ber, int *rscp, int *ecn0,
                                 int *rsrq, int *rsrp)
{
    bool success;
    int l_rxlev;
    int l_ber;
    int l_rscp;
    int l_ecn0;
    int l_rsrq;
    int l_rsrp;

    LOCK();

    MBED_ASSERT(_at != NULL);

    success = _at->send("AT+CESQ") && _at->recv("+CESQ: %d, %d, %d, %d, %d, %d\nOK\n",
                                                &l_rxlev, &l_ber, &l_rscp, &l_ecn0, &l_rsrq,
                                                &l_rsrp);

    if (success) {
        if (rxlev != NULL) {
            *rxlev = l_rxlev;
        }
        if (ber != NULL) {
            *ber = l_ber;
        }
        if (rscp != NULL) {
            *rscp = l_rscp;
        }
        if (ecn0 != NULL) {
            *ecn0 = l_ecn0;
        }
        if (rsrq != NULL) {
            *rsrq = l_rsrq;
        }
        if (rsrp != NULL) {
            *rsrp = l_rsrp;
        }
    }

    UNLOCK();
    return success;
}

#ifndef MODEM_IS_2G_3G
bool UbloxCellularBase::set_mno_profile(int mno_profile)
{
    bool success;
    char buf[32];
    LOCK();

    MBED_ASSERT(_at != NULL);

    // Set the MNO Profile, required for SARA-R4
    // SARA R4/N4 AT Command Manual UBX-17003787, section 7.14
    sprintf(buf, "AT+UMNOPROF=%d", mno_profile);
    success = _at->send(buf) && _at->recv("OK");
    tr_info("MNO Profile set to %d", mno_profile);

    UNLOCK();
    return success;
}

int UbloxCellularBase::get_mno_profile()
{
    int mno_profile = -1;
    LOCK();

    MBED_ASSERT(_at != NULL);

    // Get the MNO Profile
    // SARA R4/N4 AT Command Manual UBX-17003787, section 7.14
    _at->send("AT+UMNOPROF?") && _at->recv("+UMNOPROF: %d", &mno_profile) && _at->recv("OK");
    tr_info("MNO Profile is %d", mno_profile);

    UNLOCK();

    return mno_profile;
}

// Set the radio configuration of the modem.
void UbloxCellularBase::set_radio_config(int rat, unsigned long long int band_mask)
{
    _rat = rat;
    _band_mask = band_mask;
}

// Set the sole RAT, removing all others
bool UbloxCellularBase::set_rat(int rat)
{
    bool success = false;
    LOCK();

    MBED_ASSERT(_at != NULL);

    if (rat >= 0) {
        if (_at->send("AT+URAT=%d", rat) && _at->recv("OK")) {
            tr_info("Sole RAT is now %d", rat);
            success = true;
        }
    }

    UNLOCK();

    return success;
}

// Set the RAT of the given rank.
int UbloxCellularBase::set_rat(int rank, int rat)
{
    int rats[MAX_NUM_RATS];
    int finalRank = -1;
    char buf[32];
    bool atLeastOneDone = false;
    size_t x;

    LOCK();

    MBED_ASSERT(_at != NULL);

    if ((size_t) rank < sizeof(rats) / sizeof(rats[0])) {
        // Get the existing RATs
        for (x = 0; x < sizeof(rats) / sizeof(rats[0]); x++) {
            rats[x] = get_rat((int) x);
        }
        // Overwrite the one we want to set
        rats[rank] = rat;

        // Remove duplicates
        for (x = 0; x < sizeof(rats) / sizeof(rats[0]); x++) {
            for (size_t y = x + 1; y < sizeof(rats) / sizeof(rats[0]); y++) {
                if ((rats[x] >= 0) && (rats[x] == rats[y])) {
                    rats[y] = -1;
                }
            }
        }
        // Assemble the AT command:
        // SARA R4/N4 AT Command Manual UBX-17003787, section 7.5
        for (size_t y = 0; y < sizeof(rats) / sizeof(rats[0]); y++) {
            if (rats[y] >= 0) {
                if (rats[y] == rat) {
                    finalRank = y;
                }
                if (!atLeastOneDone) {
                    x = sprintf(buf, "AT+URAT=%d", rats[y]);
                    atLeastOneDone = true;
                } else {
                    x = sprintf(buf + x, ",%d", rats[y]);
                }
            }
        }

        // If there is at least one RAT remaining, send the command
        if (atLeastOneDone) {
            if (_at->send(buf) && _at->recv("OK")) {
                tr_info("RAT %d written at rank %d", rat, rank);
            } else {
                finalRank = -1;
            }
        }
    }

    UNLOCK();

    return finalRank;
}

// Get the selected RAT.
int UbloxCellularBase::get_rat(int rank)
{
    int rats[MAX_NUM_RATS];
    int rat = -1;
    char buf[16];
    LOCK();

    MBED_ASSERT(_at != NULL);

    // Assume there are no RATs
    for (size_t x = 0; x < sizeof(rats) / sizeof (rats[0]); x++) {
        rats[x] = -1;
    }

    // Get the RAT from SARA-R4
    // SARA R4/N4 AT Command Manual UBX-17003787, section 7.5
    _at->send("AT+URAT?") && _at->recv("+URAT: %15[^\n]\nOK\n", buf);
    // Gotta do this as a separated sscanf() as _at->recv()
    // doesn't do "best effort" matching of parameters,
    // the whole thing has to match
    sscanf(buf, "%d,%d", &(rats[0]), &(rats[1]));
    tr_info("Primary RAT is %d, secondary RAT is %d", rats[0], rats[1]);

    if ((size_t) rank < sizeof(rats) / sizeof(rats[0])) {
        rat = rats[rank];
    }

    UNLOCK();

    return rat;
}

// Set the band mask in the given RAT.
bool UbloxCellularBase::set_band_mask(int rat, unsigned long long int mask)
{
    bool success = false;
    char buf[64];
    LOCK();

    MBED_ASSERT(_at != NULL);

    if ((rat >= 7) && (rat <= 8)) {
        // Set the band mask
        // SARA R4/N4 AT Command Manual UBX-17003787, section 7.15
        sprintf(buf, "AT+UBANDMASK=%d,%llu", rat - 7, mask);
        success = _at->send(buf) && _at->recv("OK");
        tr_info("Band mask set to 0x%016llx for rat %d", mask, rat);
    } else {
        tr_error("In RAT %d; band mask can only be set for NBIoT (8) and Cat-M1 (7)", rat);
    }

    UNLOCK();
    return success;
}

// Get the band mask for the given RAT.
unsigned long long int UbloxCellularBase::get_band_mask(int rat)
{
    unsigned long long int masks[MAX_NUM_RATS];
    int rats[MAX_NUM_RATS];
    char buf1[20];
    char buf2[20];
    unsigned long long int mask = -1;
    LOCK();

    MBED_ASSERT(_at != NULL);
    if ((rat >= 7) && (rat <= 8)) {
        for (size_t x = 0; x < sizeof(masks) / sizeof (masks[0]); x++) {
            masks[x] = -1;
            rats[x] = -1;
        }

        // Get the band mask
        // SARA R4/N4 AT Command Manual UBX-17003787, section 7.15
        _at->send("AT+UBANDMASK?") &&
        _at->recv("+UBANDMASK: %d,%19[^,],%d,%19[^\n]\n",
                  &rats[0], buf1, &rats[1], buf2) &&
        _at->recv("OK");
        // Gotta do this horrible stuff 'cos, for some reason _at->recv() bombs out
        // early on the string if you ask it to do it
        sscanf(buf1, "%llu", &(masks[0]));
        sscanf(buf2, "%llu", &(masks[1]));
        for (size_t x = 0; x < sizeof(rats) / sizeof (rats[0]); x++) {
            if (rats[x] + 7 == rat) {
                mask = masks[x];
            }
            tr_info("Bandmask for %d is 0x%016llx", rats[x] + 7, masks[x]);
        }
    } else {
        tr_error("RAT given was %d; band mask can only be obtained for NBIoT (8) and Cat-M1 (7)", rat);
    }

    UNLOCK();

    return mask;
}

bool UbloxCellularBase::set_modem_reboot()
{
    bool success;
    LOCK();

    MBED_ASSERT(_at != NULL);

    // Reboot the SARA-R4 modem
    // SARA R4/N4 AT Command Manual UBX-17003787, section 5.2
    success = _at->send("AT+CFUN=15") && _at->recv("OK");
    tr_info("Modem is being rebooted.");

    UNLOCK();
    return success;
}

// Get the contents of AT+UCGED.
//
bool UbloxCellularBase::get_ucged(int *e_arfcn, int *cell_id,
                                  int *rsrq, int *rsrp)
{
    bool success;
    int l_cell_id;
    int l_e_arfcn;
    double l_rsrp;
    double l_rsrq;

    LOCK();

    MBED_ASSERT(_at != NULL);

    success = _at->send("AT+UCGED?") && _at->recv("+RSRP: %d,%d,\"%lf\",\n",
                                                  &l_cell_id, &l_e_arfcn,
                                                  &l_rsrp)
                                     && _at->recv("+RSRQ: %*d,%*d,\"%lf\",\n",
                                                  &l_rsrq)
                                     && _at->recv("OK\n");
    if (success) {
        if (e_arfcn != NULL) {
            *e_arfcn = l_e_arfcn;
        }
        if (cell_id != NULL) {
            *cell_id = l_cell_id;
        }
        if (rsrq != NULL) {
            if (l_rsrq >= 0) {
                *rsrq = (int) (l_rsrq + 0.5);
            } else {
                *rsrq = (int) (l_rsrq - 0.5);
            }
        }
        if (rsrp != NULL) {
            *rsrp = (int) (l_rsrp - 0.5);
        }
    }

    UNLOCK();
    return success;
}

// Set the CME Error callback
void UbloxCellularBase::set_cme_error_callback(Callback<void(int)> callback)
{
    _cme_error_callback = callback;
}

// Set a CSCON callback.
void UbloxCellularBase::set_cscon_callback(Callback<void(int)> callback)
{
    _cscon_callback = callback;
}

bool UbloxCellularBase::set_power_saving_mode(int periodic_time, int active_time, Callback<void(void*)> func, void *ptr)
{
    bool return_val = false;

    LOCK();
    int at_timeout = _at_timeout;
    at_set_timeout(10000); //AT+CPSMS has response time of < 10s

    if (periodic_time == 0 && active_time == 0) {
        // disable PSM
        if (_at->send("AT+CPSMS=0") && _at->recv("OK")) {
            return_val = true;
            _at->set_psm_status(false);
            _at->attach(NULL, NULL); //de-register the callback
        } else {
            return_val = false;
        }
    } else {
        /**
            Table 10.5.163a/3GPP TS 24.008: GPRS Timer 3 information element

            Bits 5 to 1 represent the binary coded timer value.

            Bits 6 to 8 defines the timer value unit for the GPRS timer as follows:
            8 7 6
            0 0 0 value is incremented in multiples of 10 minutes
            0 0 1 value is incremented in multiples of 1 hour
            0 1 0 value is incremented in multiples of 10 hours
            0 1 1 value is incremented in multiples of 2 seconds
            1 0 0 value is incremented in multiples of 30 seconds
            1 0 1 value is incremented in multiples of 1 minute
            1 1 0 value is incremented in multiples of 320 hours (NOTE 1)
            1 1 1 value indicates that the timer is deactivated (NOTE 2).
         */
        char pt[8+1];// timer value encoded as 3GPP IE
        const int ie_value_max = 0x1f;
        uint32_t periodic_timer = 0;
        if (periodic_time <= 2*ie_value_max) { // multiples of 2 seconds
            periodic_timer = periodic_time/2;
            strcpy(pt, "01100000");
        } else {
            if (periodic_time <= 30*ie_value_max) { // multiples of 30 seconds
                periodic_timer = periodic_time/30;
                strcpy(pt, "10000000");
            } else {
                if (periodic_time <= 60*ie_value_max) { // multiples of 1 minute
                    periodic_timer = periodic_time/60;
                    strcpy(pt, "10100000");
                } else {
                    if (periodic_time <= 10*60*ie_value_max) { // multiples of 10 minutes
                        periodic_timer = periodic_time/(10*60);
                        strcpy(pt, "00000000");
                    } else {
                        if (periodic_time <= 60*60*ie_value_max) { // multiples of 1 hour
                            periodic_timer = periodic_time/(60*60);
                            strcpy(pt, "00100000");
                        } else {
                            if (periodic_time <= 10*60*60*ie_value_max) { // multiples of 10 hours
                                periodic_timer = periodic_time/(10*60*60);
                                strcpy(pt, "01000000");
                            } else { // multiples of 320 hours
                                int t = periodic_time / (320*60*60);
                                if (t > ie_value_max) {
                                    t = ie_value_max;
                                }
                                periodic_timer = t;
                                strcpy(pt, "11000000");
                            }
                        }
                    }
                }
            }
        }

        uint_to_binary_str(periodic_timer, &pt[3], sizeof(pt)-3, 5);
        pt[8] = '\0';

        /**
            Table 10.5.172/3GPP TS 24.008: GPRS Timer information element

            Bits 5 to 1 represent the binary coded timer value.

            Bits 6 to 8 defines the timer value unit for the GPRS timer as follows:

            8 7 6
            0 0 0  value is incremented in multiples of 2 seconds
            0 0 1  value is incremented in multiples of 1 minute
            0 1 0  value is incremented in multiples of decihours
            1 1 1  value indicates that the timer is deactivated.

            Other values shall be interpreted as multiples of 1 minute in this version of the protocol.
        */
        char at[8+1];
        uint32_t active_timer; // timer value encoded as 3GPP IE
        if (active_time <= 2*ie_value_max) { // multiples of 2 seconds
            active_timer = active_time/2;
            strcpy(at, "00000000");
        } else {
            if (active_time <= 60*ie_value_max) { // multiples of 1 minute
                active_timer = (1<<5) | (active_time/60);
                strcpy(at, "00100000");
            } else { // multiples of decihours
                int t = active_time / (6*60);
                if (t > ie_value_max) {
                    t = ie_value_max;
                }
                active_timer = t;
                strcpy(at, "01000000");
            }
        }

        uint_to_binary_str(active_timer, &at[3], sizeof(at)-3, 5);
        at[8] = '\0';

        if (_at->send("AT+CPSMS=1,,,\"%s\",\"%s\"", pt, at) && _at->recv("OK")) {
            return_val = true;
            _at->set_psm_status(true);
            _at->attach(func, ptr);
        } else {
            tr_error("+CPSMS command failed");
            return_val = false;
        }
    }
    at_set_timeout(at_timeout);
    UNLOCK();
    return return_val;
}

void UbloxCellularBase::uint_to_binary_str(uint32_t num, char* str, int str_size, int bit_cnt)
{
    if (!str || str_size < bit_cnt) {
        return;
    }
    int tmp, pos = 0;

    for (int i = 31; i >= 0; i--) {
        tmp = num >> i;
        if (i < bit_cnt) {
            if (tmp&1) {
                str[pos] = 1 + '0';
            } else {
                str[pos] = 0 + '0';
            }
            pos++;
        }
    }
}

bool UbloxCellularBase::modem_psm_wake_up()
{
    bool success = false;
    int at_timeout;
    LOCK();

    at_timeout = _at_timeout;
    MBED_ASSERT(_at != NULL);

    for (int retry_count = 0; !success && (retry_count < 10); retry_count++) {
        if ( (retry_count % 5) == 0) {
            modem_power_up();
        }
        Thread::wait(500);
        // Modem tends to spit out noise during power up - don't confuse the parser
        _at->flush();
        at_set_timeout(1000);
        if (_at->send("AT")) {
            // C027 needs a delay here
            wait_ms(100);
            if (_at->recv("OK")) {
                success = true;
            }
        }
        at_set_timeout(at_timeout);
    }

    if (!success) {
        tr_error("modem failed to wakeup from PSM");
    }

    UNLOCK();
    return success;
}

#endif

// End of File

