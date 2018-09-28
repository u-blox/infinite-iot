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
#include <eh_utilities.h> // for MTX_LOCK()/MTX_UNLOCK()
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

/** Output pin to switch on power to some of the I2C sensors.
 * Note: if you change this for any reason, you may also
 * need to change the call to nrf_gpio_cfg() in i2cInit().
 */
static DigitalOut gEnableI2C(PIN_ENABLE_1V8, 0);

/** Remember SDA pin so that we can tidy it up on deinit().
 */
static PinName gSda;

/** Remember SCL pin so that we can tidy it up on deinit().
 */
static PinName gScl;

/**************************************************************************
 * STATIC FUNCTIONS
 *************************************************************************/

/**************************************************************************
 * PUBLIC FUNCTIONS
 *************************************************************************/

// Initialise I2C.
void i2cInit(PinName sda, PinName scl)
{
    MTX_LOCK(gMtx);

    if (gpI2c == NULL) {
        gpI2c = new I2C(sda, scl);
        gSda = sda;
        gScl = scl;

        gEnableI2C = 1;
    }

    MTX_UNLOCK(gMtx);
}

// De-initialise I2C.
void i2cDeinit()
{
    MTX_LOCK(gMtx);

    if (gpI2c != NULL) {
        delete gpI2c;

#if TARGET_UBLOX_EVK_NINA_B1
        // There is an NRF52832 chip bug which
        // leaves the current sitting at a few
        // hundred uA, see here:
        // http://infocenter.nordicsemi.com/index.jsp?topic=%2Fcom.nordic.infocenter.nrf52832.Rev1.errata%2Fanomaly_832_89.html&cp=2_1_1_1_1_26
        // To fix this, need to toggle a
        // hidden power register inside the chip
        *(volatile uint32_t *)0x40003FFC = 0;
        *(volatile uint32_t *)0x40003FFC;
        *(volatile uint32_t *)0x40003FFC = 1;

        *(volatile uint32_t *)0x40004FFC = 0;
        *(volatile uint32_t *)0x40004FFC;
        *(volatile uint32_t *)0x40004FFC = 1;

        // Now set the I2C pins to a good
        // default state to minimise current
        // draw (see SCL_PIN_INIT_CONF in nrf_drv_twi.c)
        nrf_gpio_cfg(gSda,
                     NRF_GPIO_PIN_DIR_INPUT,
                     NRF_GPIO_PIN_INPUT_DISCONNECT,
                     NRF_GPIO_PIN_NOPULL,
                     NRF_GPIO_PIN_S0D1,
                     NRF_GPIO_PIN_NOSENSE);

        nrf_gpio_cfg(gScl,
                     NRF_GPIO_PIN_DIR_INPUT,
                     NRF_GPIO_PIN_INPUT_DISCONNECT,
                     NRF_GPIO_PIN_NOPULL,
                     NRF_GPIO_PIN_S0D1,
                     NRF_GPIO_PIN_NOSENSE);
#endif

        gpI2c = NULL;
        gEnableI2C = 0;
    }

    MTX_UNLOCK(gMtx);
}

// Set the I2C bus frequency.
void i2cSetFrequency(int frequencyHz)
{
    MTX_LOCK(gMtx);

    if (gpI2c != NULL) {
        gpI2c->frequency(frequencyHz);
    }

    MTX_UNLOCK(gMtx);
}

// Send and/or receive over the I2C interface.
I2CReceivedOrError i2cSendReceive(char i2cAddress, const char *pSend,
                                  int bytesToSend, char *pReceive,
                                  int bytesReceived)
{
    I2CReceivedOrError receivedOrError;
    bool repeatedStart;

    MTX_LOCK(gMtx);

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

    MTX_UNLOCK(gMtx);

    return receivedOrError;
}

// Just do a send, with the option of doing a repeated start
// rather than a stop, over the I2C interface.
I2CReceivedOrError i2cSend(char i2cAddress, const char *pSend,
                           int bytesToSend, bool repeatedStart)
{
    I2CReceivedOrError error;

    MTX_LOCK(gMtx);

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

    MTX_UNLOCK(gMtx);

    return error;
}

// Send an I2C stop condition.
I2CReceivedOrError i2cStop()
{
    I2CReceivedOrError error;

    MTX_LOCK(gMtx);

    error = I2C_RESULT_ERROR_NOT_INITIALISED;

    if (gpI2c != NULL) {
        gpI2c->stop();
        error = I2C_RESULT_OK;
    }

    MTX_UNLOCK(gMtx);

    return error;
}

// End of file
