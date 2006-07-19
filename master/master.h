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
   EtherCAT master structure.
*/

/*****************************************************************************/

#ifndef _EC_MASTER_H_
#define _EC_MASTER_H_

#include <linux/list.h>
#include <linux/sysfs.h>
#include <linux/timer.h>

#include "device.h"
#include "domain.h"
#include "fsm.h"

/*****************************************************************************/

/**
   EtherCAT master mode.
*/

typedef enum
{
    EC_MASTER_MODE_ORPHANED,
    EC_MASTER_MODE_FREERUN,
    EC_MASTER_MODE_RUNNING
}
ec_master_mode_t;

/*****************************************************************************/

/**
   Cyclic statistics.
*/

typedef struct
{
    unsigned int timeouts; /**< datagram timeouts */
    unsigned int delayed; /**< delayed datagrams */
    unsigned int corrupted; /**< corrupted frames */
    unsigned int unmatched; /**< unmatched datagrams */
    cycles_t t_last; /**< time of last output */
}
ec_stats_t;

/*****************************************************************************/

/**
   EtherCAT master.
   Manages slaves, domains and IO.
*/

struct ec_master
{
    struct list_head list; /**< list item for module's master list */
    unsigned int reserved; /**< non-zero, if the master is reserved for RT */
    unsigned int index; /**< master index */

    struct kobject kobj; /**< kobject */

    struct list_head slaves; /**< list of slaves on the bus */
    unsigned int slave_count; /**< number of slaves on the bus */

    ec_device_t *device; /**< EtherCAT device */

    struct list_head datagram_queue; /**< datagram queue */
    uint8_t datagram_index; /**< current datagram index */

    struct list_head domains; /**< list of domains */

    ec_datagram_t simple_datagram; /**< datagram structure for initialization */
    unsigned int timeout; /**< timeout in synchronous IO */

    int debug_level; /**< master debug level */
    ec_stats_t stats; /**< cyclic statistics */

    struct workqueue_struct *workqueue; /**< master workqueue */
    struct work_struct freerun_work; /**< free run work object */
    ec_fsm_t fsm; /**< master state machine */
    ec_master_mode_t mode; /**< master mode */

    struct timer_list eoe_timer; /**< EoE timer object */
    unsigned int eoe_running; /**< non-zero, if EoE processing is active. */
    struct list_head eoe_handlers; /**< Ethernet-over-EtherCAT handlers */
    spinlock_t internal_lock; /**< spinlock used in freerun mode */
    int (*request_cb)(void *); /**< lock request callback */
    void (*release_cb)(void *); /**< lock release callback */
    void *cb_data; /**< data parameter of locking callbacks */

    uint8_t eeprom_write_enable; /**< allow write operations to EEPROMs */
};

/*****************************************************************************/

// master creation and deletion
int ec_master_init(ec_master_t *, unsigned int, unsigned int);
void ec_master_clear(struct kobject *);
void ec_master_reset(ec_master_t *);

// free run
void ec_master_freerun_start(ec_master_t *);
void ec_master_freerun_stop(ec_master_t *);

// EoE
void ec_master_eoe_start(ec_master_t *);
void ec_master_eoe_stop(ec_master_t *);

// IO
void ec_master_receive(ec_master_t *, const uint8_t *, size_t);
void ec_master_queue_datagram(ec_master_t *, ec_datagram_t *);
int ec_master_simple_io(ec_master_t *, ec_datagram_t *);

// slave management
int ec_master_bus_scan(ec_master_t *);

// misc.
void ec_master_clear_slaves(ec_master_t *);
void ec_sync_config(const ec_sync_t *, const ec_slave_t *, uint8_t *);
void ec_eeprom_sync_config(const ec_eeprom_sync_t *, const ec_slave_t *,
                           uint8_t *);
void ec_fmmu_config(const ec_fmmu_t *, const ec_slave_t *, uint8_t *);
void ec_master_output_stats(ec_master_t *);

/*****************************************************************************/

#endif
