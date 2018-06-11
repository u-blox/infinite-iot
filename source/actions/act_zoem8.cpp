/* mbed Microcontroller Library
 * Copyright (c) 2006-2018 u-blox Limited
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <mbed.h>
#include <eh_debug.h>
#include <eh_i2c.h>
#include <gnss.h> // For GnssParser
#include <act_position.h>
#include <act_zoem8.h>

/**************************************************************************
 * MANIFEST CONSTANTS
 *************************************************************************/

/**  The default message buffer size.
 */
#define DEFAULT_BUFFER_SIZE 256

/** The offset at the start of a UBX protocol message.
 */
#define GNSS_UBX_PROTOCOL_HEADER_SIZE 6

/**************************************************************************
 * TYPES
 *************************************************************************/

/** Our GNSS class, just sufficient to integrate GnssParser.
 */
class XGnssParser : public GnssParser
{
public:
    /** Constructor.
     *
     * @param i2cAddress the I2C address of the GNSS chip.
     * @param rxSize     the length of RX buffer to use.
     */
    XGnssParser(char i2cAddress, int rxSize = DEFAULT_BUFFER_SIZE);

    /** Destructor.
     */
    virtual ~XGnssParser();

    /** Init.
     */
    virtual bool init(PinName pn = NC);

    /** Get a message from the GNSS chip.
     *
     * @param pBuf to put the message into.
     * @param len  the length of pBuf.
     * @return     the number of bytes retrieved.
     */
    virtual int getMessage(char *pBuf, int len);

    /** Send an NMEA message to the GNSS chip.
     *
     * @param pBuf the message payload to write.
     * @param len  size of the message payload to write.
     * @return     total bytes written.
     */
    virtual int sendNmea(const char *pBuf, int len);

    /** Send a UBX message to the chip.
     *
     * @param cls  the UBX class id.
     * @param id   the UBX message id.
     * @param pBuf the message payload to write.
     * @param len  size of the message payload to write.
     * @return     total bytes written.
     */
    virtual int sendUbx(unsigned char cls, unsigned char id,
                        const void *pBuf = NULL, int len = 0);

protected:
    /** The I2C address of the GNSS chip.
     */
    char _i2cAddress;

    /** The receive pipe.
     */
    Pipe<char> _pipe;

    /** Write bytes to the chip.
     *
     * @param pBuf the buffer of bytes to write.
     * @param len  the number of bytes to write.
     * @return     the number of bytes written.
     */
    virtual int _send(const void *pBuf, int len);

    /** Read bytes from the chip.
     * @param pBuf the buffer to read into.
     * @param len  size of the read buffer .
     * @return     bytes read.
     */
    int _get(char *pBuf, int len);
};


/**************************************************************************
 * LOCAL VARIABLES
 *************************************************************************/

/** The instance of XGnssParser.
 */
static XGnssParser *gpGnssParser = NULL;

/// A general purpose buffer, used for sending
// and receiving UBX commands to/from the GNSS module.
static char gMsgBuffer[DEFAULT_BUFFER_SIZE];

/**************************************************************************
 * CLASSES
 *************************************************************************/

// Constructor.
XGnssParser::XGnssParser(char i2cAddress, int rxSize) :
        _pipe(rxSize)
{
    _i2cAddress = i2cAddress;
}

// Destructor
XGnssParser::~XGnssParser()
{
    powerOff();
}

// Init
bool XGnssParser::init(PinName pn)
{
    char data = 0xFF;  // REGSTREAM

    // Power up and check that we can write to the chip
    _powerOn();
    return (i2cSendReceive(_i2cAddress, &data, 1, NULL, 0) == 0);
}

// Get a message from the GNSS chip.
int XGnssParser::getMessage(char *pBuf, int len)
{
    int sz;

    // Fill the pipe
    sz = _pipe.free();

    if (sz) {
        sz = _get(pBuf, sz);
    }
    if (sz) {
        _pipe.put(pBuf, sz);
    }

    // Now parse it
    return _getMessage(&_pipe, pBuf, len);
}

// Send an NMEA message to the GNSS chip.
int XGnssParser::sendNmea(const char *pBuf, int len)
{
    int sent = 0;
    char data = 0xFF; // REGSTREAM

    if (_send(&data, 1) == 0) {
        sent = gpGnssParser->sendNmea(pBuf, len);
    }

    i2cStop();

    return sent;
}

// Send a UBX message to the GNSS chip.
int XGnssParser::sendUbx(unsigned char cls, unsigned char id, const void *pBuf, int len)
{
    int sent = 0;
    char data = 0xFF; // REGSTREAM

    if (_send(&data, 1) == 0) {
        sent = gpGnssParser->sendUbx(cls, id, pBuf, len);
    }

    i2cStop();

    return sent;
}

// Fetch up to len characters into pBuf
int XGnssParser::_get(char *pBuf, int len)
{
    int read = 0;
    int size;
    char data[3];

    data[0] = 0xFD; // REGLEN
    if (i2cSendReceive(_i2cAddress, data, 1, &(data[1]), 2) == 0) {
        size = (((int) data[1]) << 8) + (int) data[2];
        if (size > len) {
            size = len;
        }
        if (size > 0) {
            data[0] = 0xFF; // REGSTREAM
            if (i2cSendReceive(_i2cAddress, data, 1, pBuf, size) == 0) {
                read = size;
            }
        }
    }

    return read;
}

// Send.
int XGnssParser::_send(const void *pBuf, int len)
{
    return (i2cSend(_i2cAddress, (const char *) pBuf, len, true) == 0) ? len : 0;
}

/**************************************************************************
 * STATIC FUNCTIONS
 *************************************************************************/

/// Return an unsigned int from a pointer to a little-endian unsigned int
// in memory.
static unsigned int littleEndianUint(char *pByte)
{
    unsigned int retValue;

    retValue = *pByte;
    retValue += ((unsigned int) *(pByte + 1)) << 8;
    retValue += ((unsigned int) *(pByte + 2)) << 16;
    retValue += ((unsigned int) *(pByte + 3)) << 24;

    return  retValue;
}

/**************************************************************************
 * PUBLIC FUNCTIONS
 *************************************************************************/

// Initialise the Zoe M8 GNSS chip.
// TODO proper configuration for lower power mode.
ActionDriver zoem8Init(char i2cAddress)
{
    ActionDriver result = ACTION_DRIVER_OK;

    // Instantiate GnssParser and the pipe it uses
    if (gpGnssParser == NULL) {
        gpGnssParser = new XGnssParser(i2cAddress);
        if (gpGnssParser != NULL) {
            if (!gpGnssParser->init()) {
                result = ACTION_DRIVER_ERROR_DEVICE_NOT_PRESENT;
                delete gpGnssParser;
                gpGnssParser = NULL;
            }
        } else {
            result = ACTION_DRIVER_ERROR_OUT_OF_MEMORY;
        }
    }

    return result;
}

// Shut-down the Zoe M8 GNSS chip.
void zoem8Deinit()
{
    if (gpGnssParser != NULL) {
        delete gpGnssParser;
        gpGnssParser = NULL;
    }
}

// Read the position
ActionDriver getPosition(int *pLatitudeX1000, int *pLongitudeX1000,
                         int *pRadiusMetres, int *pAltitudeMetres,
                         unsigned char *pSpeedMPS)
{
    ActionDriver result = ACTION_DRIVER_ERROR_NOT_INITIALISED;
    int returnCode;

    if (gpGnssParser != NULL) {
        // See ublox8-M8_ReceiverDescrProtSpec, section 32.18.14 (NAV-PVT)
        result = ACTION_DRIVER_ERROR_I2C_WRITE;
        if (gpGnssParser->sendUbx(0x01, 0x07, NULL, 0) >= 0) {
            result = ACTION_DRIVER_ERROR_NO_DATA;
            returnCode = gpGnssParser->getMessage(gMsgBuffer, sizeof(gMsgBuffer));
            if (PROTOCOL(returnCode) == GnssParser::UBX) {
                returnCode = LENGTH(returnCode);
            }
            if (returnCode > 0) {
                result = ACTION_DRIVER_ERROR_NO_VALID_DATA;
                // Have we got a 3D fix?
                if (gMsgBuffer[20 + GNSS_UBX_PROTOCOL_HEADER_SIZE] == 0x03) {
                    result = ACTION_DRIVER_OK;
                    if ((gMsgBuffer[21 + GNSS_UBX_PROTOCOL_HEADER_SIZE] & 0x01) == 0x01) {
                        if (pLongitudeX1000 != NULL) {
                            *pLongitudeX1000 = ((int) littleEndianUint(&(gMsgBuffer[24 + GNSS_UBX_PROTOCOL_HEADER_SIZE]))) / 10000;
                        }
                        if (pLatitudeX1000 != NULL) {
                            *pLatitudeX1000 = ((int) littleEndianUint(&(gMsgBuffer[28 + GNSS_UBX_PROTOCOL_HEADER_SIZE]))) / 10000;
                        }
                        if (pAltitudeMetres != NULL) {
                            *pAltitudeMetres = ((int) littleEndianUint(&(gMsgBuffer[36 + GNSS_UBX_PROTOCOL_HEADER_SIZE]))) / 1000;
                        }
                        if (pRadiusMetres != NULL) {
                            *pRadiusMetres = ((int) littleEndianUint(&(gMsgBuffer[40 + GNSS_UBX_PROTOCOL_HEADER_SIZE]))) / 1000;
                        }
                        if (pSpeedMPS != NULL) {
                            *pSpeedMPS = ((int) littleEndianUint(&(gMsgBuffer[60 + GNSS_UBX_PROTOCOL_HEADER_SIZE]))) / 1000;
                        }
                    }
                }
            }
        }
    }

    return result;
}

// End of file
