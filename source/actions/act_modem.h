/*
 * Copyright (C) u-blox Melbourn Ltd
 * u-blox Melbourn Ltd, Melbourn, UK
 *
 * All rights reserved.
 *
 * This source file is the sole property of u-blox Melbourn Ltd.
 * Reproduction or utilisation of this source in whole or part is
 * forbidden without the written consent of u-blox Melbourn Ltd.
 */

#ifndef _ACT_MODEM_H_
#define _ACT_MODEM_H_

#include <time.h>
#include <act_common.h>

/**************************************************************************
 * MANIFEST CONSTANTS
 *************************************************************************/

/** The number of bytes of heap required to run the modem actions.
 * Ensure that this much heap is always available, irrespective of the
 * amount of data that piles up, otherwise the system will lock-up as
 * the data queue can only be emptied by transmitting it.
 */
#define MODEM_HEAP_REQUIRED_BYTES 4096

/** The number of bytes required to store an IMEI string (including terminator).
 */
#define MODEM_IMEI_LENGTH 16

/** The maximum number of report packets to send in one go.
 */
#define MODEM_MAX_NUM_REPORT_PACKETS 32

/**************************************************************************
 * FUNCTIONS
 *************************************************************************/

/** Initialise the modem.  This includes determining what kind
 * of modem (SARA-R410M or SARA-N2xx) is present.
 *
 * @param  pSimPin   a pointer to the SIM PIN.
 * @param  pApn      a pointer to the APN to use, NULL if there is none.
 * @param  pUserName a pointer to the user name to use with the APN,
 *                   NULL if there is none.
 * @param  pPassword a pointer to the password to use with the APN.
 *                   NULL if there is none.
 * @return        zero on success or negative error code on failure.
 */
ActionDriver modemInit(const char *pSimPin, const char *pApn,
                       const char *pUserName, const char *pPassword);

/** Shutdown the modem.
 */
void modemDeinit();

/** Get the IMEI from the modem.
 *
 * @param: pImei a place to store the MODEM_IMEI_LENGTH digits (inclusive of
 *               null termination, which is ensured) of IMEI string.
 * @return       zero on success or negative error code on failure.
 */
ActionDriver modemGetImei(char *pImei);

/** Make a data connection.
 *
 * @param pKeepGoingCallback a function to call back which will return
 *                           true if it's OK to keep going, else false.
 * @param pCallbackParam     a parameter to pass to keepingGoingCallback()
 *                           when it is called.
 */
ActionDriver modemConnect(bool (*pKeepGoingCallback)(void *),
                          void *pCallbackParam);

/** Get the last connect error code.
 */
int modemGetLastConnectErrorCode();

/** Get the time from an NTP server.
 *
 * @param pTimeUTC a pointer to somewhere to put the (Unix) UTC time.
 * @return         zero on success or negative error code on failure.
 */
ActionDriver modemGetTime(time_t *pTimeUTC);

/** Send reports, going through the data list and freeing
 * it up as data is sent.
 *
 * @param pIdString            the ID string to use in the reports.
 * @param pServerAddress       the IP address of the server to send to.
 * @param serverPort           the port of the server to send to.
 * @param keepingGoingCallback a function to call back which will return
 *                             true if it's OK to keep going, else false.
 * @param pCallbackParam       a parameter to pass to keepingGoingCallback()
 *                             when it is called.
 * @return                     zero on success or negative error code on
 *                             failure.
 */
ActionDriver modemSendReports(const char *pServerAddress, int serverPort,
                              const char *pIdString,
                              bool (keepingGoingCallback(void *)),
                              void *pCallbackParam);

/** Determine the type of modem attached, used during testing.
 *
 * @return true if the N2 modem is attached, else false.
 */
bool modemIsN2();

/** Determine the type of modem attached, used during testing.
 *
 * @return true if the R4 modem is attached, else false.
 */
bool modemIsR4();

/** Determine the energy consumed by the modem in nanoWatt hours.
 * Note: this is, of course, rather approximate!
 *
 * @param idleTimeSeconds  the time spent idle (not transmitting
 *                         or receiving); if this is zero then
 *                         it is assumed that the modem started
 *                         from off and so a registration cost
 *                         is added to the energy consumed.
 * @param bytesTransmitted the number of bytes transmitted.
 * @return                 the energy consumed in nanoWatt hours,
 *                         limiting at 0xFFFFFFFF on overflow.
 */
unsigned long long int modemEnergyNWH(unsigned int idleTimeSeconds,
                                      unsigned int bytesTransmitted);

#endif // _ACT_MODEM_H_

// End Of File
