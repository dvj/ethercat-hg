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
   EtherCAT interface for EtherCAT device drivers.
*/

/**
   \defgroup DeviceInterface EtherCAT device interface
   Master interface for EtherCAT-capable network device drivers.
   Through the EtherCAT device interface, EtherCAT-capable network device
   drivers are able to connect their device(s) to the master, pass received
   frames and notify the master about status changes. The master on his part,
   can send his frames through connected devices.
*/

/*****************************************************************************/

#ifndef _ETHERCAT_DEVICE_H_
#define _ETHERCAT_DEVICE_H_

#include <linux/netdevice.h>

/*****************************************************************************/

struct ec_device;
typedef struct ec_device ec_device_t; /**< \see ec_device */

/**
   Device poll function type.
*/

typedef void (*ec_pollfunc_t)(struct net_device *);

/*****************************************************************************/
// Offering/withdrawal functions

int ecdev_offer(struct net_device *net_dev, ec_device_t **,
        const char *driver_name, unsigned int board_index,
        ec_pollfunc_t poll, struct module *module);
void ecdev_withdraw(ec_device_t *device);

/*****************************************************************************/
// Device methods

int ecdev_open(ec_device_t *device);
void ecdev_close(ec_device_t *device);
void ecdev_receive(ec_device_t *device, const void *data, size_t size);
void ecdev_link_state(ec_device_t *device, uint8_t newstate);

/*****************************************************************************/

#endif
