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

#include <act_common.h>

/**************************************************************************
 * MANIFEST CONSTANTS
 *************************************************************************/

/** The number of bytes of heap required to run the modem actions.
 * Ensure that this much heap is always available, irrespective of the
 * amount of data that piles up, otherwise the system will lock-up as
 * the data queue can only be emptied by transmitting it.
 */
#define MODEM_HEAP_REQUIRED_BYTES 5000

/**************************************************************************
 * FUNCTIONS
 *************************************************************************/

/** Initialise the modem.  This includes determining what kind
 * of modem (SARA-R410M or SARA-N2xx) is present.
 *
 * @return  zero on success or negative error code on failure.
 */
ActionDriver modemInit();

/** Shutdown the modem.
 */
void modemDeinit();

/** Make a data connection.
 */
ActionDriver modemConnect();

/** Get the time from an NTP server.
 *
 * @param pTimeUtc a pointer to somewhere to put the (Unix) UTC time.
 * @return         zero on success or negative error code on failure.
 */
ActionDriver modemGetTime(time_t *pTimeUtc);

/** Send reports, going through the Data list and freeing
 * it up as data is sent.
 *
 * @return zero on success or negative error code on failure.
 */
ActionDriver modemSendReports();

#endif // _ACT_MODEM_H_

// End Of File
