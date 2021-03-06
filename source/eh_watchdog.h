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

#ifndef _EH_WATCHDOG_H_
#define _EH_WATCHDOG_H_

/**************************************************************************
 * MANIFEST CONSTANTS
 *************************************************************************/

/**************************************************************************
 * FUNCTIONS
 *************************************************************************/

/** Initialise the watchdog: can only be called once after power-on.
 *
 * @param timeoutSeconds     the watchdog timeout in seconds (max value 36 hours).
 * @param pInterruptCallback a callback that will be called two 36 kHz clock
 *                           cycles before the device is reset (may be NULL)
 * @return                   true on success, else false.
 */
bool initWatchdog(int timeoutSeconds, void (*pInterruptCallback)(void));

/** Feed the watchdog.
 */
void feedWatchdog();

#endif // _EH_WATCHDOG_H_

// End Of File
