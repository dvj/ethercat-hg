/******************************************************************************
 *
 *  $Id$
 *
 *  Copyright (C) 2006-2008  Florian Pose, Ingenieurgemeinschaft IgH
 *
 *  This file is part of the IgH EtherCAT Master.
 *
 *  The IgH EtherCAT Master is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License version 2, as
 *  published by the Free Software Foundation.
 *
 *  The IgH EtherCAT Master is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General
 *  Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with the IgH EtherCAT Master; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 *  ---
 *
 *  The license mentioned above concerns the source code only. Using the
 *  EtherCAT technology and brand is only permitted in compliance with the
 *  industrial property and similar rights of Beckhoff Automation GmbH.
 *
 *****************************************************************************/

/**
   \file
   EtherCAT slave configuration structure.
*/

/*****************************************************************************/

#ifndef __EC_SLAVE_CONFIG_H__
#define __EC_SLAVE_CONFIG_H__

#include <linux/list.h>

#include "globals.h"
#include "slave.h"
#include "sync_config.h"
#include "fmmu_config.h"

/*****************************************************************************/

/** EtherCAT slave configuration.
 */
struct ec_slave_config {
    struct list_head list; /**< List item. */
    ec_master_t *master; /**< Master owning the slave configuration. */

    uint16_t alias; /**< Slave alias. */
    uint16_t position; /**< Index after alias. If alias is zero, this is the
                         ring position. */
    uint32_t vendor_id; /**< Slave vendor ID. */
    uint32_t product_code; /**< Slave product code. */

    uint16_t watchdog_divider; /**< Watchdog divider as a number of 40ns
                                 intervals (see spec. reg. 0x0400). */
    uint16_t watchdog_intervals; /**< Process data watchdog intervals (see
                                   spec. reg. 0x0420). */
    uint8_t allow_overlapping_pdos;	/**< Allow input PDOs use the same frame space
                                      as output PDOs. */
    ec_slave_t *slave; /**< Slave pointer. This is \a NULL, if the slave is
                         offline. */

    ec_sync_config_t sync_configs[EC_MAX_SYNC_MANAGERS]; /**< Sync manager
                                                   configurations. */
    ec_fmmu_config_t fmmu_configs[EC_MAX_FMMUS]; /**< FMMU configurations. */
    uint8_t used_fmmus; /**< Number of FMMUs used. */
    unsigned int used_for_fmmu_datagram[EC_DIR_COUNT]; /**< Number of FMMUs
                                                         used for process data
                                                         exchange datagrams. */
    uint16_t dc_assign_activate; /**< Vendor-specific AssignActivate word. */
    ec_sync_signal_t dc_sync[EC_SYNC_SIGNAL_COUNT]; /**< DC sync signals. */

    struct list_head sdo_configs; /**< List of SDO configurations. */
    struct list_head sdo_requests; /**< List of SDO requests. */
    struct list_head voe_handlers; /**< List of VoE handlers. */
    struct list_head soe_configs; /**< List of SoE configurations. */
};

/*****************************************************************************/

void ec_slave_config_init(ec_slave_config_t *, ec_master_t *, uint16_t,
        uint16_t, uint32_t, uint32_t);
void ec_slave_config_clear(ec_slave_config_t *);

int ec_slave_config_attach(ec_slave_config_t *);
void ec_slave_config_detach(ec_slave_config_t *);

void ec_slave_config_load_default_sync_config(ec_slave_config_t *);

unsigned int ec_slave_config_sdo_count(const ec_slave_config_t *);
const ec_sdo_request_t *ec_slave_config_get_sdo_by_pos_const(
        const ec_slave_config_t *, unsigned int);
ec_sdo_request_t *ec_slave_config_find_sdo_request(ec_slave_config_t *,
        unsigned int);
ec_voe_handler_t *ec_slave_config_find_voe_handler(ec_slave_config_t *,
        unsigned int);

ec_sdo_request_t *ecrt_slave_config_create_sdo_request_err(
        ec_slave_config_t *, uint16_t, uint8_t, size_t);
ec_voe_handler_t *ecrt_slave_config_create_voe_handler_err(
        ec_slave_config_t *, size_t);

/*****************************************************************************/

#endif
