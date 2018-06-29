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

#ifndef _EH_UTILITIES_H_
#define _EH_UTILITIES_H_

// ----------------------------------------------------------------
// COMPILE-TIME CONSTANTS
// ----------------------------------------------------------------

/** Helper to make sure that lock/unlock pairs are always balanced.
 */
#define MTX_LOCK(x)         { x.lock()

/** Helper to make sure that lock/unlock pairs are always balanced.
 */
#define MTX_UNLOCK(x)       } x.unlock()

#ifndef ARRAY_SIZE
/** Get the number of items in an array.
 */
 #define ARRAY_SIZE(x)  (sizeof(x) / sizeof(x[0]))
#endif

#ifndef xstr
/** Stringify a macro, from
 * https://stackoverflow.com/questions/2653214/stringification-of-a-macro-value.
 * Use xstr() on a #define to turn blah into "blah", for instance, given:
 *
 * #define thingy blah
 *
 * ...then xstr(thingy) would return "blah".
 */
# define xstr(a) str(a)
# define str(a) #a
#endif

// ----------------------------------------------------------------
// VARIABLES
// ----------------------------------------------------------------

/** The number of days in each month, non leap-year.
 */
extern const unsigned char gDaysInMonth[];

/** The number of days in each month, leap-year.
 */
extern const unsigned char gDaysInMonthLeapYear[];

// ----------------------------------------------------------------
// FUNCTIONS
// ----------------------------------------------------------------

/** Check if a year is a leap year.
 *
 * @param year the year..
 * @return     true if the year was a leap year, else false.
 */
bool isLeapYear(unsigned int year);

/** Convert a hex string of a given length into a sequence of bytes, returning the
 * number of bytes written.
 *
 * @param pInBuf    pointer to the input string.
 * @param lenInBuf  length of the input string (not including any terminator).
 * @param pOutBuf   pointer to the output buffer.
 * @param lenOutBuf length of the output buffer.
 * @return          the number of bytes written.
 */
int utilitiesHexStringToBytes(const char *pInBuf, int lenInBuf, char *pOutBuf, int lenOutBuf);

/** Convert an array of bytes into a hex string, returning the number of bytes
 * written.  The hex string is NOT null terminated.
 *
 * @param pInBuf    pointer to the input buffer.
 * @param lenInBuf  length of the input buffer.
 * @param pOutBuf   pointer to the output buffer.
 * @param lenOutBuf length of the output buffer.
 * @return          the number of bytes in the output hex string.
 */
int utilitiesBytesToHexString(const char *pInBuf, int lenInBuf, char *pOutBuf, int lenOutBuf);

#endif // _EH_UTILITIES_H_

// End Of File
