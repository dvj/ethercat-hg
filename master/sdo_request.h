/******************************************************************************
 *
 *  $Id$
 *
 *  Copyright (C) 2006  Florian Pose, Ingenieurgemeinschaft IgH
 *
 *  This file is part of the IgH EtherCAT Master.
 *
 *  The IgH EtherCAT Master is free software; you can redistribute it
 *  and/or modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2 of the
 *  License, or (at your option) any later version.
 *
 *  The IgH EtherCAT Master is distributed in the hope that it will be
 *  useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with the IgH EtherCAT Master; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 *  The right to use EtherCAT Technology is granted and comes free of
 *  charge under condition of compatibility of product made by
 *  Licensee. People intending to distribute/sell products based on the
 *  code, have to sign an agreement to guarantee that products using
 *  software based on IgH EtherCAT master stay compatible with the actual
 *  EtherCAT specification (which are released themselves as an open
 *  standard) as the (only) precondition to have the right to use EtherCAT
 *  Technology, IP and trade marks.
 *
 *****************************************************************************/

/**
   \file
   EtherCAT CANopen Sdo request structure.
*/

/*****************************************************************************/

#ifndef __EC_SDO_REQUEST_H__
#define __EC_SDO_REQUEST_H__

#include <linux/list.h>

#include "globals.h"

/*****************************************************************************/

/** CANopen Sdo request.
 */
typedef struct {
    struct list_head list; /**< List item. */
    uint16_t index; /**< Sdo index. */
    uint8_t subindex; /**< Sdo subindex. */
    uint8_t *data; /**< Pointer to Sdo data. */
    size_t mem_size; /**< Size of Sdo data memory. */
    size_t data_size; /**< Size of Sdo data. */
    ec_request_state_t state; /**< Sdo request state. */
} ec_sdo_request_t;

/*****************************************************************************/

void ec_sdo_request_init(ec_sdo_request_t *);
void ec_sdo_request_clear(ec_sdo_request_t *);

void ec_sdo_request_address(ec_sdo_request_t *, uint16_t, uint8_t);
int ec_sdo_request_alloc(ec_sdo_request_t *, size_t);
int ec_sdo_request_copy_data(ec_sdo_request_t *, const uint8_t *, size_t);

void ec_sdo_request_read(ec_sdo_request_t *);
void ec_sdo_request_write(ec_sdo_request_t *);

/*****************************************************************************/

#endif
