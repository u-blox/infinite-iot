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

#ifndef _ACT_CELLULAR_H_
#define _ACT_CELLULAR_H_

#include <act_common.h>

/**************************************************************************
 * MANIFEST CONSTANTS
 *************************************************************************/

/**************************************************************************
 * TYPES
 *************************************************************************/

/**************************************************************************
 * FUNCTIONS
 *************************************************************************/

/** Get the received cellular signal strengths.
 *
 * @param pRsrpDbm    a place to put the strength of the wanted signal in dBm.
 * @param pRssiDbm    a place to put the received signal strength of in dBm.
 * @param pRsrqDb     a place to put the received signal quality in dB;
 *                    see 3GPP 36.214.
 * @param pSnrDbm     a place to put the the received signal to noise ratio in dBm.
 * @return            zero on success or negative error code on failure.
 */
ActionDriver getCellularSignalRx(int *pRsrpDbm, int *pRssiDbm,
                                 int *pRsrqDb, int *pSnrDbm);

/** Get the cellular transmit signal strength.
 *
 * @param pPowerDbm   a place to put the transmit power in dBm.
 * @return            zero on success or negative error code on failure.
 */
ActionDriver getCellularSignalTx(int *pPowerDbm);

/** Get the cellular channel parameters.
 *
 * @param pCellId  a place to put the cell ID, which is unique across the
 *                 network.
 * @param pEarfcn  a place to put the EARFCN.
 * @param pEcl     a place to put the coverage class (0 to 2).
 * @return         zero on success or negative error code on failure.
 */
ActionDriver getCellularChannel(unsigned int *pCellId, unsigned int *pEarfcn,
                                unsigned char *pEcl);

#endif // _ACT_CELLULAR_H_

// End Of File
