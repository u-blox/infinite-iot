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

#include "UbloxATCmdParser.h"
#include "rtos/Thread.h"

using namespace mbed;

UbloxATCmdParser::UbloxATCmdParser(FileHandle *fh, const char *output_delimiter, int buffer_size, int timeout, bool debug) :
_function(0), _cb_param(0), _psm_status(0), ATCmdParser(fh, output_delimiter, buffer_size, timeout, debug)
{

}
UbloxATCmdParser::~UbloxATCmdParser()
{

}

bool UbloxATCmdParser::send(const char *command, ...)
{
    bool ret_val = false;

    if (_psm_status == true) {
        if ( (ATCmdParser::send("AT")) && (ATCmdParser::recv("OK")) ) {
            ret_val = true;
        } else {
            if (_function) {
                _function(_cb_param); //modem is asleep, execute the application registered cb to notify
            }
        }
    }

    va_list args;
    va_start(args, command);
    ret_val = ATCmdParser::vsend(command, args);
    va_end(args);
    return ret_val;
}

void UbloxATCmdParser::set_psm_status(bool state)
{
    _psm_status = state;
}

bool UbloxATCmdParser::get_psm_status()
{
    return _psm_status;
}

#endif /* MODEM_IS_2G_3G */
