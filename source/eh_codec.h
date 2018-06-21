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

#ifndef _EH_CODEC_H_
#define _EH_CODEC_H_

/**************************************************************************
 * MANIFEST CONSTANTS
 *************************************************************************/

/** The minimum size of encode buffer: smaller than this and there is a risk
 * that the largest data item (DataLog) might not be encodable at all under
 * worst case conditions, causing it to get stuck in the data list.
 */
#define CODEC_ENCODE_BUFFER_MIN_SIZE 1024

/**************************************************************************
 * TYPES
 *************************************************************************/

/**************************************************************************
 * FUNCTIONS
 *************************************************************************/

/** Prepare the data for coding, which simply means sort it.
 */
void codecPrepareData();

/** Encode as much queued data as will fit into a buffer. Data should
 * be prepared before the first call (with a call to codecPrepareData()).
 * After encoding, the data pointers are left such that pDataNext() is
 * pointing to the next data item which would be encoded, any data items
 * not requiring an acknowledgement being freed in the process.
 * Hence the correct pattern is:
 *
 * codecPrepareData();
 * while ((len = codecEncodeData(nameString, buf, sizeof(buf))) > 0) {
 *    // Do something with the len bytes of data encoded into buf
 * }
 * codecAckData();
 *
 * @param pNameString the name of this device, which will be encoded at the
 *                    start of each report.
 * @param pBuf        a pointer to the buffer to encode into.
 * @param len         the length of pBuf.
 * @return            the number of bytes encoded or -1 if there are data items
 *                    to encode but pBuf is not big enough to encode even one
 *                    of them; to avoid this condition always offer a pBuf
 *                    at least CODEC_ENCODE_BUFFER_MIN_SIZE bytes big.
 */
int codecEncodeData(const char *pNameString, char *pBuf, int len);

/** This function should be called after codecEncodeData() once the encoded buffer
 * has been sent in order to free any data items which were marked as requiring
 * acknowledgement.  If it is not called, the data items will remain in the
 * data queue to be encoded on a subsequent call to codecEncodeData().
 * NOTE: obviously for this to work no pDataNext(), pDataFirst() or pDataSort()
 * calls must be made between the call to codecEncodeData() and this call.
 */
void codecAckData();

#endif // _EH_CODEC_H_

// End Of File
