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
#define LOCK(x)         { x.lock()

/** Helper to make sure that lock/unlock pairs are always balanced.
 */
#define UNLOCK(x)       } x.unlock()

#ifndef ARRAY_SIZE
/** Get the number of items in an array.
 */
  #define ARRAY_SIZE(x)  (sizeof(x) / sizeof(x[0]))
#endif

// ----------------------------------------------------------------
// FUNCTIONS
// ----------------------------------------------------------------

/** Convert a hex string of a given length into a sequence of bytes, returning the
 * number of bytes written.
 *
 * @param pInBuf    pointer to the input string.
 * @param lenInBuf  length of the input string (not including any terminator).
 * @param pOutBuf   pointer to the output buffer.
 * @param lenOutBuf length of the output buffer.
 * @return          the number of bytes written.
 */
int hexStringToBytes(const char *pInBuf, int lenInBuf, char *pOutBuf, int lenOutBuf);

/** Convert an array of bytes into a hex string, returning the number of bytes
 * written.  The hex string is NOT null terminated.
 *
 * @param pInBuf    pointer to the input buffer.
 * @param lenInBuf  length of the input buffer.
 * @param pOutBuf   pointer to the output buffer.
 * @param lenOutBuf length of the output buffer.
 * @return          the number of bytes in the output hex string.
 */
int bytesToHexString(const char *pInBuf, int lenInBuf, char *pOutBuf, int lenOutBuf);

#endif // _EH_UTILITIES_H_

// End Of File
