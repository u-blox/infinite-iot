/* Copyright (c) 2017 ARM Limited
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

#ifndef _UBLOX_CELLULAR_BASE_
#define _UBLOX_CELLULAR_BASE_

#include "mbed.h"
#ifndef MODEM_IS_2G_3G
#include "UbloxATCmdParser.h"
#else
#include "ATCmdParser.h"
#endif
#include "FileHandle.h"

/**********************************************************************
 * CLASSES
 **********************************************************************/

/** UbloxCellularBase class.
 *
 *  This class provides all the base support for generic u-blox modems
 *  on C030 and C027 boards: module identification, power-up, network
 *  registration, etc.
 */
class UbloxCellularBase {

public:
    /** Circuit Switched network registration status (CREG Usage).
     * UBX-13001820 - AT Commands Example Application Note (Section 7.10.3).
     */
    typedef enum {
        CSD_NOT_REGISTERED_NOT_SEARCHING = 0,
        CSD_REGISTERED = 1,
        CSD_NOT_REGISTERED_SEARCHING = 2,
        CSD_REGISTRATION_DENIED = 3,
        CSD_UNKNOWN_COVERAGE = 4,
        CSD_REGISTERED_ROAMING = 5,
        CSD_SMS_ONLY = 6,
        CSD_SMS_ONLY_ROAMING = 7,
        CSD_CSFB_NOT_PREFERRED = 9
    } NetworkRegistrationStatusCsd;

    /** Packet Switched network registration status (CGREG Usage).
     * UBX-13001820 - AT Commands Example Application Note (Section 18.27.3).
     */
    typedef enum {
        PSD_NOT_REGISTERED_NOT_SEARCHING = 0,
        PSD_REGISTERED = 1,
        PSD_NOT_REGISTERED_SEARCHING = 2,
        PSD_REGISTRATION_DENIED = 3,
        PSD_UNKNOWN_COVERAGE = 4,
        PSD_REGISTERED_ROAMING = 5,
        PSD_EMERGENCY_SERVICES_ONLY = 8
    } NetworkRegistrationStatusPsd;

    /** EPS network registration status (CEREG Usage).
     * UBX-13001820 - AT Commands Example Application Note (Section 18.36.3).
     */
    typedef enum {
        EPS_NOT_REGISTERED_NOT_SEARCHING = 0,
        EPS_REGISTERED = 1,
        EPS_NOT_REGISTERED_SEARCHING = 2,
        EPS_REGISTRATION_DENIED = 3,
        EPS_UNKNOWN_COVERAGE = 4,
        EPS_REGISTERED_ROAMING = 5,
        EPS_EMERGENCY_SERVICES_ONLY = 8
    } NetworkRegistrationStatusEps;

    /** Initialise the modem, ready for use.
     *
     *  @param pin     PIN for the SIM card.
     *  @return        true if successful, otherwise false.
     */
    bool init(const char *pin = 0);

    /** Perform registration with the network.
     *
     * @param keepingGoingCallback a function to call back which will return
     *                             true if it's OK to keep going, else false.
     * @param callbackParam        a parameter to pass to keepingGoingCallback()
     *                             when it is called.
     * @param watchdogCallback     watchdog callback in case registration takes
     *                             a long time.
     * @return                     true on success, otherwise fals.
     */
    bool nwk_registration(bool (keepingGoingCallback) (void *),
                          void *callbackParam,
                          void (*watchdogCallback) (void));

    /** True if the modem is registered for circuit
     * switched data, otherwise false.
     */
    bool is_registered_csd();

    /** True if the modem is registered for packet
     * switched data, otherwise false.
     */
    bool is_registered_psd();

    /** True if the modem is registered for enhanced
     * packet switched data (i.e. LTE and beyond),
     * otherwise false.
     */
    bool is_registered_eps();

    /** Perform deregistration from the network.
     *
     * @return true if successful, otherwise false.
     */
    bool nwk_deregistration();

    /** Put the modem into its lowest power state.
     */
    void deinit();

    /** Set the PIN code for the SIM card.
     *
     *  @param pin PIN for the SIM card.
     */
    void set_pin(const char *pin);

    /** Enable or disable SIM pin checking.
     *
     * @param enableNotDisable true if SIM PIN checking is to be enabled,
     *                         otherwise false.
     * @return                 true if successful, otherwise false.
     */
    bool sim_pin_check_enable(bool enableNotDisable);

    /** Change the SIM pin.
     *
     * @param new_pin the new PIN to use.
     * @return        true if successful, otherwise false.
     */
    bool change_sim_pin(const char *new_pin);

    /** Get the IMEI of the module.
     *
     * @return a pointer to the IMEI as a null-terminated string.
     */
    const char *imei();

    /** Get the Mobile Equipment ID (which may be the same as the IMEI).
     *
     * @return a pointer to the Mobile Equipment ID as a null-terminated string.
     */
    const char *meid();

    /** Get the IMSI of the SIM.
     *
     * @return a pointer to the IMSI as a null-terminated string.
     */
    const char *imsi();

    /** Get the ICCID of the SIM.
     *
     * @return a pointer to the ICCID as a null-terminated string.
     */
    const char *iccid();

    /** Get the RSSI.
     *
     * @return the RSSI in dBm. If it is not possible to obtain an
     *         RSSI reading at the time (e.g. because the modem is in
     *         data mode rather than AT command mode) then 0 is returned.
     */
    int rssi();

    /** Get the contents of AT+CESQ.
     *
     * @param rxlev a place to put the rxlev.
     * @param ber   a place to put the BER.
     * @param rscp  a place to put the receive signal code power.
     * @param ecn0  a place to put the ECN0.
     * @param rsrq  a place to put the RSRQ.
     * @param rsrp  a place to put the RSRP.
     * @return      true on success, otherwise false.
     */
    bool get_cesq(int *rxlev, int *ber, int *rscp, int *ecn0,
                  int *rsrq, int *rsrp);

    /** Set a CME Error callback.
     *
     * @param the callback.
     */
    void set_cme_error_callback(Callback<void(int)> callback);

#ifndef MODEM_IS_2G_3G
    /** Get the contents of AT+UCGED.
     *
     * @param e_arfcn  a place to put the EARFCN.
     * @param cell_id  a place to put the physical cell ID.
     * @param rsrq     a place to put the RSRQ (4G only.
     * @param rsrp     a place to put the RSRP (4G only).
     * @return         true on success, otherwise false.
     */
    bool get_ucged(int *e_arfcn, int *cell_id,
                   int *rsrq, int *rsrp);

    /** Set a CSCON callback.
     *
     * @param the callback.
     */
    void set_cscon_callback(Callback<void(int)> callback);

    /** Set the MNO Profile (need to reboot for this to take effect)
     *
     * @param mno_profile the profile number to set.
     * @return            true if successful, else false.
     */
    bool set_mno_profile(int mno_profile);

    /** Get the MNO Profile.
     *
     * @return the profile number.
     */
    int get_mno_profile();

    /** Set the radio configuration of the R4 modem
     *
     * @param p_rat           the primary RAT: 7 for cat-M1,
     *                        8 for NBIoT, -1 for leave alone.
     * @param p_rat_band_mask the band mask for the primary RAT
     *                        where bit 0 is band 1 and bit 63
     *                        is band 64.  Must be present if
     *                        p_rat is not -1.
     * @param s_rat           the secondary RAT: 7 for cat-M1,
     *                        8 for NBIoT, -1 for leave alone.
     *                        Will be ignored if p_rat is -1.
     * @param s_rat_band_mask the band mask for the secondary RAT
     *                        where bit 0 is band 1 and bit 63
     *                        is band 64.  Must be present if
     *                        both p_rat and s_rat are not -1.
     */
    void set_radio_config(int p_rat, unsigned long long int p_rat_band_mask,
                          int s_rat, unsigned long long int s_rat_band_mask);

    /** Set the RAT of the given rank. Note: the rank
     * that results may be different if duplicates have
     * to be removed when writing.  For example, if the
     * current RATs are 7 & 8 and set_rat() is given
     * rank 7 at rank 1 then the result will be a single
     * RAT, 7, at rank 0.
     *
     * @param rank the rank, where 0 is the primary RAT,
     *             1 the secondary RAT, etc.
     * @return     the resulting rank, -1 on failure.
     */
    int set_rat(int rank, int rat);

    /** Get the RAT of the given rank.
     *
     * @param rank the rank, where 0 is the primary RAT,
     *             1 the secondary RAT, etc.
     * @return     the RAT (e.g. 7 for Cat-M1, 8 for NBIoT)
     *             -1 if the rank does not exist.
     */
    int get_rat(int rank);

    /** Set the band mask for the given RAT.
     *
     * @param rat  the RAT.
     * @param mask bitmap for bands 64 to 1.
     * @return     true on success, else false.
     */
    bool set_band_mask(int rat, unsigned long long int mask);

    /** Get the band mask for the given RAT.
     *
     * @param  the RAT.
     * @return the bitmap for bands 64 to 1, 0
     *         if the RAT is not supported.
     */
    unsigned long long int get_band_mask(int rat);

    /** Enable or disable the 3GPP PSM. Application should reboot the module
     * after enabling PSM in order to enter PSM state
     *
     * @param periodic_time    requested periodic TAU in seconds.
     * @param active_time      requested active time in seconds.
     * @param func             callback function to execute when modem goes to sleep
     * @param ptr              parameter to callback function
     * @return                 true if successful, otherwise false.
     */
    bool set_power_saving_mode(int periodic_time, int active_time, Callback<void(void*)> func = NULL, void *ptr = NULL);

    /** Converts the given uint to binary string. Fills the given str starting
     *  from [0] with the number of bits defined by bit_cnt
     *  For example uint_to_binary_string(9, str, 10) would fill str "0000001001"
     *  For example uint_to_binary_string(9, str, 3) would fill str "001"
     *
     *  @param num       uint to converts to binary string
     *  @param str       buffer for converted binary string
     *  @param str_size  size of the str buffer
     *  @param bit_cnt   defines how many bits are filled to buffer started from lsb
     */
    void uint_to_binary_str(uint32_t num, char* str, int str_size, int bit_cnt);

    /** Wake the modem up from PSM
     *
     *  @return    True if modem has successfully waken up, false if modem could not wake up
     */
    bool modem_psm_wake_up();
#endif

protected:

    #define OUTPUT_ENTER_KEY  "\r"

    #if MBED_CONF_UBLOX_CELL_GEN_DRV_AT_PARSER_BUFFER_SIZE
    #define AT_PARSER_BUFFER_SIZE   MBED_CONF_UBLOX_CELL_GEN_DRV_AT_PARSER_BUFFER_SIZE
    #else
    #define AT_PARSER_BUFFER_SIZE   256
    #endif

    #if MBED_CONF_UBLOX_CELL_GEN_DRV_AT_PARSER_TIMEOUT
    #define AT_PARSER_TIMEOUT       MBED_CONF_UBLOX_CELL_GEN_DRV_AT_PARSER_TIMEOUT
    #else
    #define AT_PARSER_TIMEOUT       8*1000 // Milliseconds
    #endif

    /** A string that would not normally be sent by the modem on the AT interface.
     */
    #define UNNATURAL_STRING "\x01"

    /** The maximum number of RATs, used for multi-RAT operations across
     * NBIoT and Cat-M1.  Note: if you ever change this you WILL have
     * to change the AT-parsing code to match.
     */
    #define MAX_NUM_RATS 2

    /** Supported u-blox modem variants.
     */
    typedef enum {
        DEV_TYPE_NONE = 0,
        DEV_SARA_G35,
        DEV_LISA_U2,
        DEV_LISA_U2_03S,
        DEV_SARA_U2,
        DEV_SARA_R4,
        DEV_LEON_G2,
        DEV_TOBY_L2,
        DEV_MPCI_L2
    } DeviceType;

    /** Network registration status.
     * UBX-13001820 - AT Commands Example Application Note (Section 4.1.4.5).
     */
    typedef enum {
       GSM = 0,
       COMPACT_GSM = 1,
       UTRAN = 2,
       EDGE = 3,
       HSDPA = 4,
       HSUPA = 5,
       HSDPA_HSUPA = 6,
       LTE = 7,
       EC_GSM_IoT = 8,
       E_UTRAN_NB_S1 = 9
    } RadioAccessNetworkType;

    /** Info about the modem.
     */
    typedef struct {
        DeviceType dev;
        char iccid[20 + 1];   //!< Integrated Circuit Card ID.
        char imsi[15 + 1];    //!< International Mobile Station Identity.
        char imei[15 + 1];    //!< International Mobile Equipment Identity.
        char meid[18 + 1];    //!< Mobile Equipment IDentifier.
        volatile RadioAccessNetworkType rat;  //!< Type of network (e.g. 2G, 3G, LTE).
        volatile NetworkRegistrationStatusCsd reg_status_csd; //!< Circuit switched attach status.
        volatile NetworkRegistrationStatusPsd reg_status_psd; //!< Packet switched attach status.
        volatile NetworkRegistrationStatusEps reg_status_eps; //!< Evolved Packet Switched (e.g. LTE) attach status.
    } DeviceInfo;

    /* IMPORTANT: the variables below are available to
     * classes that inherit this in order to keep things
     * simple. However, ONLY this class should free
     * any of the pointers, or there will be havoc.
     */

    /** Point to the instance of the AT parser in use.
     */
#ifndef MODEM_IS_2G_3G
    UbloxATCmdParser *_at;
#else
    ATCmdParser *_at;
#endif

    /** The current AT parser timeout value.
     */
    int _at_timeout;

    /** File handle used by the AT parser.
     */
    FileHandle *_fh;

    /** The mutex resource.
     */
    Mutex _mtx;

    /** General info about the modem as a device.
     */
    DeviceInfo _dev_info;

    /** The SIM PIN to use.
     */
    const char *_pin;

    /** Set to true to spit out debug traces.
     */
    bool _debug_trace_on;
    
    /** The baud rate to the modem.
     */
    int _baud;

    /** The primary RAT.
     */
    int _p_rat;

    /** The band mask for the primary RAT.
     */
    unsigned long long int _p_rat_band_mask;

    /** The secondary RAT.
     */
    int _s_rat;

    /** The band mask for the secondary RAT.
     */
    unsigned long long int _s_rat_band_mask;

    /** True if the modem is ready register to the network,
     * otherwise false.
     */
    bool _modem_initialised;

    /** True it the SIM requires a PIN, otherwise false.
     */
    bool _sim_pin_check_enabled;

    /** Callback in case CME Error occurs.
     */
    Callback<void(int)> _cme_error_callback;

    /** Callback for connection state.
     */
    Callback<void(int)> _cscon_callback;

    /** Sets the modem up for powering on
     *
     *  modem_init() is equivalent to plugging in the device, e.g., attaching power and serial port.
     *  Uses onboard_modem_api.h where the implementation of onboard_modem_api is provided by the target.
     */
    virtual void modem_init();

    /** Sets the modem in unplugged state
     *
     *  modem_deinit() will be equivalent to pulling the plug off of the device, i.e., detaching power
     *  and serial port. This puts the modem in lowest power state.
     *  Uses onboard_modem_api.h where the implementation of onboard_modem_api is provided by the target.
     */
    virtual void modem_deinit();

    /** Powers up the modem
     *
     *  modem_power_up() is equivalent to pressing the soft power button.
     *  The driver may repeat this if the modem is not responsive to AT commands.
     *  Uses onboard_modem_api.h where the implementation of onboard_modem_api is provided by the target.
     */
    virtual void modem_power_up();

    /** Powers down the modem
     *
     *  modem_power_down() is equivalent to turning off the modem by button press.
     *  Uses onboard_modem_api.h where the implementation of onboard_modem_api is provided by the target.
     */
    virtual void modem_power_down();

    /* Note: constructor and destructor protected so that this
    * class can only ever be inherited, never used directly.
    */
    UbloxCellularBase();
    ~UbloxCellularBase();

    /** Initialise this class.
     *
     * @param tx       the UART TX data pin to which the modem is attached.
     * @param rx       the UART RX data pin to which the modem is attached.
     * @param baud     the UART baud rate.
     * @param debug_on true to switch AT interface debug on, otherwise false.
     *
     * Note: it would be more natural to do this in the constructor
     * however, to avoid the diamond of death, this class is only
     * every inherited virtually.  Classes that are inherited virtually
     * do not get passed parameters in their constructor and hence
     * classInit() must be called separately by the first one to wake
     * the beast.
     */
    void baseClassInit(PinName tx = MDMTXD,
                       PinName rx = MDMRXD,
                       int baud = MBED_CONF_UBLOX_CELL_BAUD_RATE,
                       bool debug_on = false);

    /** Set the AT parser timeout.
     */
    void at_set_timeout(int timeout);

    /** Read up to size characters from buf
     * or until the character "end" is reached, overwriting
     * the newline with 0 and ensuring a terminator
     * in all cases.
     *
     * @param buf  the buffer to write to.
     * @param size the size of the buffer.
     * @param end  the character to stop at.
     * @return     the number of characters read,
     *             not including the terminator.
     */
    int read_at_to_char(char * buf, int size, char end);

    /** Powers up the modem.
     *
     * @return true if successful, otherwise false.
     */
    bool power_up();

    /** Power down the modem.
     */
    void power_down();

    /** Lock a mutex when accessing the modem.
     */
    #define MTX_DEBUG

    #ifdef MTX_DEBUG
    void lock(void)     { _mtx.lock();}
    #else
    void lock(void)     { _mtx.lock();}
    #endif

    /** Helper to make sure that lock unlock pair is always balanced
     */
    #define LOCK()         { lock()

    /** Unlock the modem when done accessing it.
     */
    #ifdef MTX_DEBUG
    void unlock(void)     { _mtx.unlock();}
    #else
    void unlock(void)     { _mtx.unlock();}
    #endif

    /** Helper to make sure that lock unlock pair is always balanced
     */
    #define UNLOCK()       } unlock()

    /** Set the device identity in _dev_info
     * based on the ATI string returned by
     * the module.
     *
     * @return true if dev is a known value,
     *         otherwise false.
     */
    bool set_device_identity(DeviceType *dev);

    /** Perform any modem initialisation that is
     * specialised by device type.
     *
     * @return true if successful, otherwise false.
     */
    bool device_init(DeviceType dev);

    /** Set up the SIM.
     *
     * @return true if successful, otherwise false.
     */
    bool initialise_sim_card();

    /** Perform the pre-initialisation steps,
     * which is to power up and check various stored
     * configuration items.
     *
     * @param mno_profile    the intended MNO profile.
     * @param p_rat          the optional primary RAT (7 for cat-M1,
     *                       8 for NBIoT).
     * @param p_band_mask    bit mask for the primary RAT, bands 64 to
     *                       1, must be present if p_rat is present.
     * @param s_rat          the optional secondary RAT
     * @param s_band_mask    bit mask for the secondary RAT, must
     *                       be present if s_rat is present.
     * @return               true if successful, otherwise false.
     */
    bool pre_init(int mno_profile,
                  int p_rat = -1, unsigned long long int p_band_mask = 0,
                  int s_rat = -1, unsigned long long int s_band_mask = 0);

private:
    int ascii_to_int(const char *buf);
    void set_nwk_reg_status_csd(int status);
    void set_nwk_reg_status_psd(int status);
    void set_nwk_reg_status_eps(int status);
    void rat(int AcTStatus);
    bool get_iccid();
    bool get_imsi();
    bool get_imei();
    bool get_meid();
    bool set_sms();
    bool set_modem_reboot();
    void parser_abort_cb();
    void CMX_ERROR_URC();
    void CREG_URC();
    void CGREG_URC();
    void CEREG_URC();
    void UMWI_URC();
};

#endif // _UBLOX_CELLULAR_BASE_

