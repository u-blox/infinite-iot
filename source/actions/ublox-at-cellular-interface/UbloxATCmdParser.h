/*
 * Copyright (c) 2017, Arm Limited and affiliates.
 * SPDX-License-Identifier: Apache-2.0
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

#ifndef MODEM_IS_2G_3G

#ifndef _UBLOX_AT_CMD_PARSER_H_
#define _UBLOX_AT_CMD_PARSER_H_

#include "ATCmdParser.h"
#include "mbed.h"
#include "onboard_modem_api.h"

namespace mbed {

/**
 *  Class UbloxATCmdParser
 *
 */
class UbloxATCmdParser : public ATCmdParser
{
public:
    UbloxATCmdParser(FileHandle *fh, const char *output_delimiter = "\r",
             int buffer_size = 256, int timeout = 8000, bool debug = false);
    ~UbloxATCmdParser();

    void attach(Callback<void(void*)> func, void *cb_param)
    {
        _function = func;
        _cb_param = cb_param;
    }

    /**
     * Sends an AT command
     *
     * Sends a formatted command using printf style formatting
     * @see printf
     *
     * @param command printf-like format string of command to send which
     *                is appended with a newline
     * @param ...     all printf-like arguments to insert into command
     * @return        true only if command is successfully sent
     */
    bool send(const char *command, ...) MBED_PRINTF_METHOD(1,2);

    /**
     * Setter for PSM status flag
     *
     * @param status true or false
     */
    void set_psm_status(bool);

    /**
     * Getter for PSM status flag
     *
     * @return _psm_status
     */
    bool get_psm_status();

    /**
     * Set the timeout waiting
     * for a response (passed on to
     * ATCmdParser).
     *
     * @param timeout the timeout in seconds
     */
    void set_timeout(int timeout);

private:
    int _timeout;
    bool _psm_status;
    void *_cb_param;
    Callback<void(void*)> _function;  /**< Callback. */
};

}// namespace mbed

#endif /* _UBLOX_AT_CMD_PARSER_H_ */

#endif /* MODEM_IS_2G_3G */
