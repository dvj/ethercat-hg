/******************************************************************************
 *
 *  e t h e r n e t . h
 *
 *  Ethernet-over-EtherCAT (EoE)
 *
 *  $Id$
 *
 *  Copyright (C) 2006  Florian Pose, Ingenieurgemeinschaft IgH
 *
 *  This file is part of the IgH EtherCAT Master.
 *
 *  The IgH EtherCAT Master is free software; you can redistribute it
 *  and/or modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; version 2 of the License.
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
 *****************************************************************************/

#include <linux/list.h>

#include "../include/ecrt.h"
#include "globals.h"
#include "slave.h"
#include "command.h"

/*****************************************************************************/

/**
   \defgroup EoE EtherCAT-over-Ethernet (EoE)
   Data types and functions for Ethernet-over-EtherCAT.
   \{
*/

/*****************************************************************************/

/**
   State of an EoE object.
*/

typedef enum
{
    EC_EOE_IDLE,     /**< Idle. The next step ist to check for data. */
    EC_EOE_CHECKING, /**< Checking frame was sent. */
    EC_EOE_FETCHING  /**< There is new data. Fetching frame was sent. */
}
ec_eoe_state_t;

/*****************************************************************************/

/**
   Ethernet-over-EtherCAT (EoE) Object.
   The master creates one of these objects for each slave that supports the
   EoE protocol.
*/

typedef struct
{
    struct list_head list; /**< list item */
    ec_slave_t *slave; /**< pointer to the corresponding slave */
    ec_eoe_state_t rx_state; /**< state of the state machine */
}
ec_eoe_t;

/** \} */

/*****************************************************************************/

void ec_eoe_init(ec_eoe_t *, ec_slave_t *);
void ec_eoe_clear(ec_eoe_t *);
void ec_eoe_run(ec_eoe_t *);
void ec_eoe_print(const ec_eoe_t *);

/*****************************************************************************/
