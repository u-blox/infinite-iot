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
 * MANIFEST CONSTANTS: all derived by Phil Ware from his spreadsheets.
 *************************************************************************/

/** The power consumed, in nanoWatts, while the modem is off,
 * which is zero as we switch the supply off.
 */
#define CELLULAR_POWER_OFF_NW 0

/** The power consumed, in nanoWatts, while the R410 modem is in
 * standby: 10 uA @ 3.6 V.
 */
#define CELLULAR_R410_POWER_IDLE_NW 36000

/** The power consumed, in nanoWatts, while the N2XX modem is in
 * standby: 3 uA @ 3.6 V.
 */
#define CELLULAR_N2XX_POWER_IDLE_NW 10800

/** The power consumed, in nanoWatts, while the R410 modem is active
 * in receive: 98 mA @ 3.6 V.
 */
#define CELLULAR_R410_POWER_RX_ACTIVE_NW 352800000

/** The power consumed, in nanoWatts, while the N2XX modem is active
 * in receive: 48 mA @ 3.6 V.
 */
#define CELLULAR_N2XX_POWER_RX_ACTIVE_NW 172800000

/** The energy required, in nWh, for the R410 modem to transmit x bytes.
 *
 * Phil�s calculator:
 *
 * Wake up & Send X bytes = 0.025 * X + 17.5 uWh
 *                          + RRC Wait time @ 98mA (Nutaq box = 10 seconds)
 *                          + RRC Release 185 uWh
 */
#define CELLULAR_R410_ENERGY_TX_NWH(x) (((((unsigned int) x) * 1000) * 25 + 17500000) / 1000 + 35280000 + 185000)

/** The energy required, in nWh, for the N2xx modem to transmit x bytes.
 *
 * Phil�s calculator:
 *
 * Scan & RRC connection = 34 uWh
 * Send X Bytes = 0.05894 * X + 11.54 uWh
 *                + RRC wait time @ 48mA (Nutaq box  = 10 seconds)
 */
#define CELLULAR_N2XX_ENERGY_TX_NWH(x) (34000 + ((((unsigned int) x) * 1000) * 59 + 11540000) / 1000 + 172800000)

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
