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

#ifndef _EC_SLAVE_H_
#define _EC_SLAVE_H_

#include <linux/list.h>
#include <linux/kobject.h>

#include "../include/ecrt.h"

#include "globals.h"
#include "datagram.h"
#include "pdo.h"
#include "sync.h"

/*****************************************************************************/

/** Slave state mask.
 *
 * Apply this mask to a slave state byte to get the slave state without
 * the error flag.
 */
#define EC_SLAVE_STATE_MASK 0x0F

/*****************************************************************************/

/** State of an EtherCAT slave.
 */
typedef enum {
    EC_SLAVE_STATE_UNKNOWN = 0x00,
    /**< unknown state */
    EC_SLAVE_STATE_INIT = 0x01,
    /**< INIT state (no mailbox communication, no IO) */
    EC_SLAVE_STATE_PREOP = 0x02,
    /**< PREOP state (mailbox communication, no IO) */
    EC_SLAVE_STATE_SAFEOP = 0x04,
    /**< SAFEOP (mailbox communication and input update) */
    EC_SLAVE_STATE_OP = 0x08,
    /**< OP (mailbox communication and input/output update) */
    EC_SLAVE_STATE_ACK_ERR = 0x10
    /**< Acknowledge/Error bit (no actual state) */
} ec_slave_state_t;

/*****************************************************************************/

/** EtherCAT slave online state.
 */
typedef enum {
    EC_SLAVE_OFFLINE,
    EC_SLAVE_ONLINE
} ec_slave_online_state_t;

/*****************************************************************************/

/** Supported mailbox protocols.
 */
enum {
    EC_MBOX_AOE = 0x01, /**< ADS-over-EtherCAT */
    EC_MBOX_EOE = 0x02, /**< Ethernet-over-EtherCAT */
    EC_MBOX_COE = 0x04, /**< CANopen-over-EtherCAT */
    EC_MBOX_FOE = 0x08, /**< File-Access-over-EtherCAT */
    EC_MBOX_SOE = 0x10, /**< Servo-Profile-over-EtherCAT */
    EC_MBOX_VOE = 0x20  /**< Vendor specific */
};

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
    ec_slave_online_state_t online_state; /**< online state */
    unsigned int self_configured; /**< Slave was configured by this master. */
    unsigned int error_flag; /**< Stop processing after an error. */

    // base data
    uint8_t base_type; /**< slave type */
    uint8_t base_revision; /**< revision */
    uint16_t base_build; /**< build number */
    uint16_t base_fmmu_count; /**< number of supported FMMUs */

    // data link status
    uint8_t dl_link[4]; /**< link detected */
    uint8_t dl_loop[4]; /**< loop closed */
    uint8_t dl_signal[4]; /**< detected signal on RX port */

    // EEPROM
    uint8_t *eeprom_data; /**< Complete EEPROM image */
    size_t eeprom_size; /**< size of the EEPROM contents in bytes */

    // slave information interface
    uint16_t sii_alias; /**< configured station alias */
    uint32_t sii_vendor_id; /**< vendor id */
    uint32_t sii_product_code; /**< vendor's product code */
    uint32_t sii_revision_number; /**< revision number */
    uint32_t sii_serial_number; /**< serial number */
    uint16_t sii_rx_mailbox_offset; /**< mailbox address (master to slave) */
    uint16_t sii_rx_mailbox_size; /**< mailbox size (master to slave) */
    uint16_t sii_tx_mailbox_offset; /**< mailbox address (slave to master) */
    uint16_t sii_tx_mailbox_size; /**< mailbox size (slave to master) */
    uint16_t sii_mailbox_protocols; /**< supported mailbox protocols */
    uint8_t sii_physical_layer[4]; /**< port media */
    char **sii_strings; /**< strings in EEPROM categories */
    unsigned int sii_string_count; /**< number of EEPROM strings */
    ec_sync_t *sii_syncs; /**< EEPROM SYNC MANAGER categories */
    unsigned int sii_sync_count; /**< number of sync managers in EEPROM */
    struct list_head sii_pdos; /**< EEPROM [RT]XPDO categories */
    char *sii_group; /**< slave group acc. to EEPROM */
    char *sii_image; /**< slave image name acc. to EEPROM */
    char *sii_order; /**< slave order number acc. to EEPROM */
    char *sii_name; /**< slave name acc. to EEPROM */
    int16_t sii_current_on_ebus; /**< power consumption */

    struct kobject sdo_kobj; /**< kobject for Sdos */
    struct list_head sdo_dictionary; /**< Sdo dictionary list */
    uint8_t sdo_dictionary_fetched; /**< dictionary has been fetched */
    unsigned long jiffies_preop; /**< time, the slave went to PREOP */
};

/*****************************************************************************/

// slave construction/destruction
int ec_slave_init(ec_slave_t *, ec_master_t *, uint16_t, uint16_t);
void ec_slave_destroy(ec_slave_t *);

void ec_slave_request_state(ec_slave_t *, ec_slave_state_t);
void ec_slave_set_state(ec_slave_t *, ec_slave_state_t);
void ec_slave_set_online_state(ec_slave_t *, ec_slave_online_state_t);

// SII categories
int ec_slave_fetch_sii_strings(ec_slave_t *, const uint8_t *, size_t);
int ec_slave_fetch_sii_general(ec_slave_t *, const uint8_t *, size_t);
int ec_slave_fetch_sii_syncs(ec_slave_t *, const uint8_t *, size_t);
int ec_slave_fetch_sii_pdos(ec_slave_t *, const uint8_t *, size_t,
        ec_direction_t);

// misc.
ec_sync_t *ec_slave_get_pdo_sync(ec_slave_t *, ec_direction_t); 
int ec_slave_validate(const ec_slave_t *, uint32_t, uint32_t);
void ec_slave_sdo_dict_info(const ec_slave_t *,
        unsigned int *, unsigned int *);
ec_sdo_t *ec_slave_get_sdo(ec_slave_t *, uint16_t);
const ec_pdo_t *ec_slave_find_pdo(const ec_slave_t *, uint16_t);

/*****************************************************************************/

#endif
