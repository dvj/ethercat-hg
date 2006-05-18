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
   Mailbox functionality.
*/

/*****************************************************************************/

#ifndef _EC_MAILBOX_H_
#define _EC_MAILBOX_H_

#include "slave.h"

/*****************************************************************************/

uint8_t *ec_slave_mbox_prepare_send(ec_slave_t *, uint8_t, size_t);
int      ec_slave_mbox_prepare_check(ec_slave_t *);
int      ec_slave_mbox_check(const ec_slave_t *);
int      ec_slave_mbox_prepare_fetch(ec_slave_t *);
uint8_t *ec_slave_mbox_fetch(ec_slave_t *, uint8_t, size_t *);

uint8_t *ec_slave_mbox_simple_io(ec_slave_t *, size_t *);
uint8_t *ec_slave_mbox_simple_receive(ec_slave_t *, uint8_t, size_t *);

/*****************************************************************************/

#endif
