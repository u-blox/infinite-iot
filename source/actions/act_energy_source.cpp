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
#include <eh_config.h>
#include <act_energy_source.h>

/**************************************************************************
 * MANIFEST CONSTANTS
 *************************************************************************/

/**************************************************************************
 * LOCAL VARIABLES
 *************************************************************************/

// Digital output to switch on energy source 1
static DigitalOut gEnableEnergySouce1(PIN_ENABLE_ENERGY_SOURCE_1, 0);

// Digital output to switch on energy source 2
static DigitalOut gEnableEnergySouce2(PIN_ENABLE_ENERGY_SOURCE_2, 0);

// Digital output to switch on energy source 3
static DigitalOut gEnableEnergySouce3(PIN_ENABLE_ENERGY_SOURCE_3, 0);

/**************************************************************************
 * STATIC FUNCTIONS
 *************************************************************************/

/**************************************************************************
 * PUBLIC FUNCTIONS
 *************************************************************************/

// End of file
