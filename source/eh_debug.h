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

#ifndef _EH_DEBUG_H_
#define _EH_DEBUG_H_

/**************************************************************************
 * MANIFEST CONSTANTS
 *************************************************************************/

// Define this to enable printing out of the serial port.  This is normally
// off because (a) the only serial port is connected to the cellular modem and
// (b) if that is not the case and you want to connect to a PC instead but
// you don't happen to have a USB cable connected at the time then everything
// will hang.
#ifdef MBED_CONF_APP_ENABLE_PRINTF
# define PRINTF(format, ...) printf(format, ##__VA_ARGS__)
#else
# define PRINTF(...)
#endif

/**************************************************************************
 * FUNCTIONS
 *************************************************************************/

/** Initialise debug.
 */
void debugInit();

/** Pulse the debug LED for a number of milliseconds.
 *
 * @param the duration of the LED pulse.
 */
void debugPulseLed(int milliseconds);

/** Flash out the "victory" LED pattern a given number of times.
 *
 * @param the number of flashes.
 */
void debugVictoryLed(int count);

/** Indicate that a debugBad thing has happened, where the thing
 * is identified by the number of pulses.
 *
 * @param: the number of pulses.
 */
void debugBad(int pulses);

#ifdef MBED_CONF_APP_ENABLE_RAM_STATS
/** Printf() out some RAM stats.
 * */
void debugPrintRamStats();
#endif

#endif // _EH_DEBUG_H_

// End Of File
