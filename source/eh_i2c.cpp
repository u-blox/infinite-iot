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

#include <mbed.h> // for I2C
#include <eh_config.h> // For PIN_ENABLE_1V8
#include <eh_utilities.h> // for LOCK()/UNLOCK()
#include <eh_i2c.h>

/**************************************************************************
 * MANIFEST CONSTANTS
 *************************************************************************/

/**************************************************************************
 * LOCAL VARIABLES
 *************************************************************************/

/** The I2C instance.
 */
static I2C *gpI2c = NULL;

/** Mutex to protect the I2C hardware.
 */
static Mutex gMtx;

// Output pin to switch on power to some of the I2C sensors.
static DigitalOut gEnableI2C(PIN_ENABLE_1V8);

/**************************************************************************
 * STATIC FUNCTIONS
 *************************************************************************/

/**************************************************************************
 * PUBLIC FUNCTIONS
 *************************************************************************/

// Initialise I2C.
void i2cInit(PinName sda, PinName scl)
{
    LOCK(gMtx);

    if (gpI2c == NULL) {
        gpI2c = new I2C(sda, scl);
        gEnableI2C = true;
    }

    UNLOCK(gMtx);
}

// De-initialise I2C.
void i2cDeinit()
{
    LOCK(gMtx);

    if (gpI2c != NULL) {
        delete gpI2c;
        gEnableI2C = false;
        gpI2c = NULL;
    }

    UNLOCK(gMtx);
}

// Set the I2C bus frequency.
void i2cSetFrequency(int frequencyHz)
{
    LOCK(gMtx);

    if (gpI2c != NULL) {
        gpI2c->frequency(frequencyHz);
    }

    UNLOCK(gMtx);
}

// Send and/or receive over the I2C interface.
I2CReceivedOrError i2cSendReceive(char i2cAddress, const char *pSend,
                                  int bytesToSend, char *pReceive,
                                  int bytesReceived)
{
    I2CReceivedOrError receivedOrError;
    bool repeatedStart;

    LOCK(gMtx);

    receivedOrError = I2C_RESULT_ERROR_NOT_INITIALISED;
    // Set repeated start if there is something to receive
    repeatedStart = (pReceive != NULL);

    if (gpI2c != NULL) {
        if ((i2cAddress & 0x80) != 0) {
            receivedOrError = I2C_RESULT_ERROR_INVALID_PARAMETER;
        } else if ((pSend == NULL) && (bytesToSend != 0)) {
            receivedOrError = I2C_RESULT_ERROR_INVALID_PARAMETER;
        } else if ((pReceive == NULL) && (bytesReceived != 0)) {
            receivedOrError = I2C_RESULT_ERROR_INVALID_PARAMETER;
        } else {
            // Mbed uses an 8-bit address, shifted up from 7
            i2cAddress <<= 1;
            if (pSend != NULL) {
                if (gpI2c->write(i2cAddress, pSend, bytesToSend, repeatedStart) != 0) {
                    receivedOrError = I2C_RESULT_ERROR_SEND_FAILED;
                }
            }
            if (receivedOrError != I2C_RESULT_ERROR_SEND_FAILED) {
                if (pReceive != NULL) {
                    if (gpI2c->read(i2cAddress, pReceive, bytesReceived) == 0) {
                        receivedOrError = bytesReceived;
                    } else {
                        receivedOrError = I2C_RESULT_ERROR_RECEIVE_FAILED;
                    }
                } else {
                    receivedOrError = I2C_RESULT_OK;
                }
            }
        }
    }

    UNLOCK(gMtx);

    return receivedOrError;
}

// Just do a send, with the option of doing a repeated start
// rather than a stop, over the I2C interface.
I2CReceivedOrError i2cSend(char i2cAddress, const char *pSend,
                           int bytesToSend, bool repeatedStart)
{
    I2CReceivedOrError error;

    LOCK(gMtx);

    error = I2C_RESULT_ERROR_NOT_INITIALISED;

    if (gpI2c != NULL) {
        if ((i2cAddress & 0x80) != 0) {
            error = I2C_RESULT_ERROR_INVALID_PARAMETER;
        } else if ((pSend == NULL) && (bytesToSend != 0)) {
            error = I2C_RESULT_ERROR_INVALID_PARAMETER;
        } else {
            // Mbed uses an 8-bit address, shifted up from 7
            i2cAddress <<= 1;
            if (pSend != NULL) {
                if (gpI2c->write(i2cAddress, pSend, bytesToSend, repeatedStart) == 0) {
                    error = I2C_RESULT_OK;
                } else {
                    error = I2C_RESULT_ERROR_SEND_FAILED;
                }
            }
        }
    }

    UNLOCK(gMtx);

    return error;
}

// Send an I2C stop condition.
I2CReceivedOrError i2cStop()
{
    I2CReceivedOrError error;

    LOCK(gMtx);

    error = I2C_RESULT_ERROR_NOT_INITIALISED;

    if (gpI2c != NULL) {
        gpI2c->stop();
        error = I2C_RESULT_OK;
    }

    UNLOCK(gMtx);

    return error;
}

// End of file
