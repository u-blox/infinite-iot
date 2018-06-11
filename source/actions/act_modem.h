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
 * @param pTime a pointer to somewhere to put the (Unix) time.
 * @return      zero on success or negative error code on failure.
 */
ActionDriver modemGetTime(time_t *pTime);

/** Send reports, going through the Data list and freeing
 * it up as data is ent.
 *
 * @return zero on success or negative error code on failure.
 */
ActionDriver modemSendReports();

#endif // _ACT_MODEM_H_

// End Of File
