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

/** A marker to use when checking for buffer overflows.
 */
#define MARKER "DEADBEEF"

/** The size of MARKER.
 */
#define MARKER_SIZE 9

/** A macro to check that the above marker is present.
 */
#define CHECK_MARKER(pMarker) (((*((pMarker) + 0) == 'D') && \
                                (*((pMarker) + 1) == 'E') && \
                                (*((pMarker) + 2) == 'A') && \
                                (*((pMarker) + 3) == 'D') && \
                                (*((pMarker) + 4) == 'B') && \
                                (*((pMarker) + 5) == 'E') && \
                                (*((pMarker) + 6) == 'E') && \
                                (*((pMarker) + 7) == 'F') && \
                                (*((pMarker) + 8) == 0)) ? true : false)

/** A macro to check that an array of a given size has MARKER at
 * both ends.
 */
#define CHECK_ARRAY(pArray, sizeArray) (CHECK_MARKER((pArray) - MARKER_SIZE) && \
                                        CHECK_MARKER((pArray) + (sizeArray)))

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
 * @para: the number of pulses.
 */
void debugBad(int pulses);

/** printf() out some RAM stats (but only if
 * MBED_CONF_APP_ENABLE_RAM_STATS is true).
 */
void debugPrintRamStats();

/** Get the heap left (but only if
 * MBED_CONF_APP_ENABLE_RAM_STATS is true).
 *
 * @return the number of bytes of heap left.
 */
int debugGetHeapLeft();

/** Get the minimum heap left (but only if
 * MBED_CONF_APP_ENABLE_RAM_STATS is true).
 *
 * @return the minimum number of bytes of heap
 *         left.
 */
int debugGetHeapMinLeft();

/** Get the stack left (but only if
 * MBED_CONF_APP_ENABLE_RAM_STATS is true).
 *
 * @return the number of bytes of stack left.
 */
int debugGetStackLeft();

#endif // _EH_DEBUG_H_

// End Of File
