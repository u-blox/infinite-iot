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

/**************************************************************************
 * TYPES
 *************************************************************************/

/**************************************************************************
 * FUNCTIONS
 *************************************************************************/

/** Encode as much queued data as will fit into a buffer. The data
 * queue is always sorted before encoding and, after encoding, the data
 * pointers are left such that pDataNext() is pointing to the next data
 * item which would be encoded, any data items not requiring an
 * acknowledgement being freed in the process.
 *
 * @param pBuf a pointer to the buffer to encode into.
 * @param len  the length of pBuf.
 * @return     the number of bytes encoded.
 */
int codecEncodeData(char *pBuf, int len);

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
