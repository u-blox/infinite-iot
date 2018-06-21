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

/** The encoded data will look something like this:
 *
 * {
 *    "n":"357520071700641","i":0,"r":{
 *        "loc":{
 *            "t":1527172040,"uWh":134,
 *            "d":{
 *                "lat":52.223117,"lng":-0.074391,"rad":5,"alt":65,"spd":0
 *            }
 *        },
 *        "lux":{
 *            "t":1527172340,"uWh":597,
 *            "d":{
 *                "lux":1004
 *            }
 *        }
 *    }
 * }
 *
 * ...where:
 *
 * n is the name (or ID) of the reporting device.
 * i is the index number of this report (in the range 0 to 0x7FFFFFFF).
 * r is the report, see the implementation for possible contents.
 *
 * If the encoded data is received by a server then the server shall send
 * back an acknowledgement of the following form:
 *
 * {"n":"357520071700641","i":0}
 *
 * ...where:
 *
 * n is the name (or ID) of the reporting device.
 * i is the index number of the report being acknowledged.
 */

/**************************************************************************
 * MANIFEST CONSTANTS
 *************************************************************************/

/** The minimum size of encode buffer: smaller than this and there is a risk
 * that the largest data item (DataLog) might not be encodable at all under
 * worst case conditions, causing it to get stuck in the data list.
 */
#define CODEC_ENCODE_BUFFER_MIN_SIZE 1024

/** The maximum length of the name field of the message.  This is used
 * to limit the search length when decoding an ack message.  It is up to the
 * caller to ensure that the name string passed into codecEncodeData()
 * is not longer than this.
 * If you change this you MUST also change the number in codecDecodeAck().
 */
#define CODEC_MAX_NAME_STRLEN 32

/**************************************************************************
 * TYPES
 *************************************************************************/

/** The possible decode errors from the message codec with a placeholder
 * to make sure that this has the full int range (when it is an index rather
 * than an error).
 */
typedef enum {
    CODEC_ERROR_NOT_ENOUGH_ROOM = -1,
    CODEC_ERROR_BAD_PARAMETER = -2,
    CODEC_ERROR_NOT_ACK_MSG = -3,
    CODEC_ERROR_NO_NAME_MATCH = -4,
    MAX_NUM_CODEC_ERROR = 0x7fffffff
} CodecErrorOrIndex;

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
 *                    start of each report, a string which should be no more
 *                    than CODEC_MAX_NAME_STRLEN bytes long (excluding
 *                    terminator).
 * @param pBuf        a pointer to the buffer to encode into.
 * @param len         the length of pBuf.
 * @return            the number of bytes encoded or -1 if there are data items
 *                    to encode but pBuf is not big enough to encode even one
 *                    of them; to avoid this condition always offer a pBuf
 *                    at least CODEC_ENCODE_BUFFER_MIN_SIZE bytes big.
 */
CodecErrorOrIndex codecEncodeData(const char *pNameString, char *pBuf, int len);

/** This function should be called after codecEncodeData() once the encoded buffer
 * has been sent in order to free any data items which were marked as requiring
 * acknowledgement.  If it is not called, the data items will remain in the
 * data queue to be encoded on a subsequent call to codecEncodeData().
 * NOTE: obviously for this to work no pDataNext(), pDataFirst() or pDataSort()
 * calls must be made between the call to codecEncodeData() and this call.
 */
void codecAckData();

/** Decode a buffer that is expected to contain an ack message of the form:
 *
 * {"n":"357520071700641","i":0}
 *
 * ...where:
 *
 * n is the name (or ID) of the reporting device.
 * i is the index number of the report being acknowledged.
 *
 * @param pBuf        a pointer to the buffer to decode.
 * @param len         the length of pBuf.
 * @param pNameString the name string to expect in the name field of the JSON
 *                    message.
 * @return            the index number, if the JSON message proves to be an
 *                    acknowledgement message and the name string matches that
 *                    given, otherwise negative to indicate an error.
 */
CodecErrorOrIndex codecDecodeAck(char *pBuf, int len, const char *pNameString);

#endif // _EH_CODEC_H_

// End Of File
