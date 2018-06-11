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

/** Get the received signal strengths.
 *
 * @param pRsrpDbm    a place to put the strength of the wanted signal in dBm.
 * @param pRssiDbm    a place to put the received signal strength of in dBm.
 * @param pRsrq       a place to put the received signal quality.
 * @param pSnrDbm     a place to put the the received signal to noise ratio in dBm.
 * @param pEclDbm     a place to put the coupling loss in dBm.
 * @return            zero on success or negative error code on failure.
 */
ActionDriver getSignalStrengthRx(int *pRsrpDbm, int *pRssiDbm,
                                 int *pRsrq, int *pSnrDbm, int *pEclDbm);

/** Get the transmit signal strength.
 *
 * @param pPowerDbm   a place to put the transmit power in dBm.
 * @return            zero on success or negative error code on failure.
 */
ActionDriver getSignalStrengthTx(int *pPowerDbm);

/** Get the channel parameters.
 *
 * @param pPhysicalCellId  a place to put the physical Cell ID.
 * @param pPci             a place to put the PCI.
 * @param pEarfcn          a place to put the EARFCN.
 * @return                 zero on success or negative error code on failure.
 */
ActionDriver getChannel(int *pPhysicalCellId, int *pPci, int *pEarfcn);

#endif // _ACT_CELLULAR_H_

// End Of File
