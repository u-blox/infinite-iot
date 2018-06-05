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

#ifndef _EH_I2C_H_
#define _EH_I2C_H_

/**************************************************************************
 * MANIFEST CONSTANTS
 *************************************************************************/

/**************************************************************************
 * TYPES
 *************************************************************************/

/** Return value for i2cSendReceive: if negative indicates an error,
 * if positive the number of bytes received.
 */
typedef signed int I2CReceivedOrError;

/** Types of results that can be returned.
 */
typedef enum {
    I2C_RESULT_OK = 0,
    I2C_RESULT_ERROR_NOT_INITIALISED = -1,
    I2C_RESULT_ERROR_INVALID_PARAMETER = -2,
    I2C_RESULT_ERROR_SEND_FAILED = -3,
    I2C_RESULT_ERROR_RECEIVE_FAILED = -4
} I2CResult;

/**************************************************************************
 * FUNCTIONS
 *************************************************************************/

/** Initialise I2C.
 *
 * @param sda the data pin.
 * @param sdc the clock pin.
 */
void i2cInit(PinName sda, PinName sdc);

/** Shutdown I2C.
 */
void i2cDeinit();

/**  Set the I2C bus frequency.
 *
 * @param frequencyHz the clock frequency.
 */
void i2cSetFrequency(int frequencyHz);

/** Send and/or receive over the I2C interface.
 *
 * @param i2cAddress     the 7-bit I2C address to exchange data with; the top
 *                       bit must be 0.
 * @param pSend          the number of bytes to send, may be NULL if only receive
 *                       is required.
 * @param bytesToSend    the number of bytes to send, must be zero if pSend is NULL.
 * @param pReceive       a pointer to a buffer in which to store received bytes,
 *                       may be NULL if only send is required.
 * @param bytesToReceive the size of buffer pointed to by pReceive, must be zero
 *                       if pReceive is NULL.
 * @return               the number of bytes received or negative error code.
 */
I2CReceivedOrError i2cSendReceive(char i2cAddress, const char *pSend,
                                  int bytesToSend, char *pReceive,
                                  int bytesReceived);

#endif // _EH_I2C_H_

// End Of File
