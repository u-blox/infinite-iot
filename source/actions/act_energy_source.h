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

#ifndef _ACT_ENERGY_SOURCE_H_
#define _ACT_ENERGY_SOURCE_H_

/**************************************************************************
 * MANIFEST CONSTANTS
 *************************************************************************/

/** The number of energy sources.
 */
#define ENERGY_SOURCES_MAX_NUM 3

/**************************************************************************
 * FUNCTIONS
 *************************************************************************/

/** Set the energy source, 0 for none, 1 for source 1,
 * 2 for source 2, etc.
 *
 * @param the source to use.
 */
void setEnergySource(unsigned char source);

/** Get the active energy source.
 *
 * @return the active energy source, 1, 2, or 3, zero
 *         if none.
 */
unsigned char getEnergySource();

#endif // _ACT_ENERGY_SOURCE_H_

// End Of File
