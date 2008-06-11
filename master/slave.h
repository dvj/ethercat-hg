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
   EtherCAT slave structure.
*/

/*****************************************************************************/

#ifndef __EC_SLAVE_H__
#define __EC_SLAVE_H__

#include <linux/list.h>
#include <linux/kobject.h>

#include "../include/ecrt.h"

#include "globals.h"
#include "datagram.h"
#include "pdo.h"
#include "sync.h"
#include "sdo.h"

/*****************************************************************************/

/** Slave information interface data.
 */
typedef struct {
    // Non-category data 
    uint16_t alias; /**< Configured station alias. */
    uint32_t vendor_id; /**< Vendor ID. */
    uint32_t product_code; /**< Vendor-specific product code. */
    uint32_t revision_number; /**< Revision number. */
    uint32_t serial_number; /**< Serial number. */
    uint16_t rx_mailbox_offset; /**< Mailbox address (master to slave). */
    uint16_t rx_mailbox_size; /**< Mailbox size (master to slave). */
    uint16_t tx_mailbox_offset; /**< Mailbox address (slave to master). */
    uint16_t tx_mailbox_size; /**< Mailbox size (slave to master). */
    uint16_t mailbox_protocols; /**< Supported mailbox protocols. */

    // Strings
    char **strings; /**< Strings in SII categories. */
    unsigned int string_count; /**< number of SII strings */

    // General
    unsigned int has_general; /**< General category present. */
    char *group; /**< slave group acc. to SII */
    char *image; /**< slave image name acc. to SII */
    char *order; /**< slave order number acc. to SII */
    char *name; /**< slave name acc. to SII */
    uint8_t physical_layer[4]; /**< port media */
    ec_sii_coe_details_t coe_details; /**< CoE detail flags. */
    ec_sii_general_flags_t general_flags; /**< General flags. */
    int16_t current_on_ebus; /**< power consumption */

    // SyncM
    ec_sync_t *syncs; /**< SII SYNC MANAGER categories */
    unsigned int sync_count; /**< number of sync managers in SII */

    // [RT]XPDO
    struct list_head pdos; /**< SII [RT]XPDO categories */
} ec_sii_t;

/*****************************************************************************/

/** EtherCAT slave.
 */
struct ec_slave
{
    struct list_head list; /**< list item */
    struct kobject kobj; /**< kobject */
    ec_master_t *master; /**< master owning the slave */

    // addresses
    uint16_t ring_position; /**< ring position */
    uint16_t station_address; /**< configured station address */

    // configuration
    ec_slave_config_t *config; /**< Current configuration. */
    ec_slave_state_t requested_state; /**< Requested application state. */
    ec_slave_state_t current_state; /**< Current application state. */
    unsigned int error_flag; /**< Stop processing after an error. */
    unsigned int force_config; /**< Force (re-)configuration. */

    // base data
    uint8_t base_type; /**< slave type */
    uint8_t base_revision; /**< revision */
    uint16_t base_build; /**< build number */
    uint16_t base_fmmu_count; /**< number of supported FMMUs */

    // data link status
    uint8_t dl_link[4]; /**< link detected */
    uint8_t dl_loop[4]; /**< loop closed */
    uint8_t dl_signal[4]; /**< detected signal on RX port */

    // SII
    uint16_t *sii_words; /**< Complete SII image. */
    size_t sii_nwords; /**< Size of the SII contents in words. */

    // slave information interface
    ec_sii_t sii; /**< SII data. */

    struct kobject sdo_kobj; /**< kobject for Sdos */
    struct list_head sdo_dictionary; /**< Sdo dictionary list */
    uint8_t sdo_dictionary_fetched; /**< dictionary has been fetched */
    unsigned long jiffies_preop; /**< time, the slave went to PREOP */
};

/*****************************************************************************/

// slave construction/destruction
int ec_slave_init(ec_slave_t *, ec_master_t *, uint16_t, uint16_t);
void ec_slave_destroy(ec_slave_t *);

void ec_slave_clear_sync_managers(ec_slave_t *);

void ec_slave_request_state(ec_slave_t *, ec_slave_state_t);
void ec_slave_set_state(ec_slave_t *, ec_slave_state_t);

// SII categories
int ec_slave_fetch_sii_strings(ec_slave_t *, const uint8_t *, size_t);
int ec_slave_fetch_sii_general(ec_slave_t *, const uint8_t *, size_t);
int ec_slave_fetch_sii_syncs(ec_slave_t *, const uint8_t *, size_t);
int ec_slave_fetch_sii_pdos(ec_slave_t *, const uint8_t *, size_t,
        ec_direction_t);

// misc.
ec_sync_t *ec_slave_get_pdo_sync(ec_slave_t *, ec_direction_t); 
void ec_slave_sdo_dict_info(const ec_slave_t *,
        unsigned int *, unsigned int *);
ec_sdo_t *ec_slave_get_sdo(ec_slave_t *, uint16_t);
const ec_sdo_t *ec_slave_get_sdo_const(const ec_slave_t *, uint16_t);
const ec_sdo_t *ec_slave_get_sdo_by_pos_const(const ec_slave_t *, uint16_t);
uint16_t ec_slave_sdo_count(const ec_slave_t *);
const ec_pdo_t *ec_slave_find_pdo(const ec_slave_t *, uint16_t);

int ec_slave_write_sii(ec_slave_t *, uint16_t, unsigned int,
        const uint16_t *);

/*****************************************************************************/

#endif
