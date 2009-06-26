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
   EtherCAT master character device IOCTL commands.
*/

/*****************************************************************************/

#ifndef __EC_IOCTL_H__
#define __EC_IOCTL_H__

#include <linux/ioctl.h>

#include "globals.h"

/*****************************************************************************/

/** \cond */

#define EC_IOCTL_TYPE 0xa4

#define EC_IO(nr)           _IO(EC_IOCTL_TYPE, nr)
#define EC_IOR(nr, type)   _IOR(EC_IOCTL_TYPE, nr, type)
#define EC_IOW(nr, type)   _IOW(EC_IOCTL_TYPE, nr, type)
#define EC_IOWR(nr, type) _IOWR(EC_IOCTL_TYPE, nr, type)

// Command-line tool
#define EC_IOCTL_MASTER                EC_IOR(0x00, ec_ioctl_master_t)
#define EC_IOCTL_SLAVE                EC_IOWR(0x01, ec_ioctl_slave_t)
#define EC_IOCTL_SLAVE_SYNC           EC_IOWR(0x02, ec_ioctl_slave_sync_t)
#define EC_IOCTL_SLAVE_SYNC_PDO       EC_IOWR(0x03, ec_ioctl_slave_sync_pdo_t)
#define EC_IOCTL_SLAVE_SYNC_PDO_ENTRY EC_IOWR(0x04, ec_ioctl_slave_sync_pdo_entry_t)
#define EC_IOCTL_DOMAIN               EC_IOWR(0x05, ec_ioctl_domain_t)
#define EC_IOCTL_DOMAIN_FMMU          EC_IOWR(0x06, ec_ioctl_domain_fmmu_t)
#define EC_IOCTL_DOMAIN_DATA          EC_IOWR(0x07, ec_ioctl_domain_data_t)
#define EC_IOCTL_MASTER_DEBUG           EC_IO(0x08)
#define EC_IOCTL_SLAVE_STATE           EC_IOW(0x09, ec_ioctl_slave_state_t)
#define EC_IOCTL_SLAVE_SDO            EC_IOWR(0x0a, ec_ioctl_slave_sdo_t)
#define EC_IOCTL_SLAVE_SDO_ENTRY      EC_IOWR(0x0b, ec_ioctl_slave_sdo_entry_t)
#define EC_IOCTL_SLAVE_SDO_UPLOAD     EC_IOWR(0x0c, ec_ioctl_slave_sdo_upload_t)
#define EC_IOCTL_SLAVE_SDO_DOWNLOAD   EC_IOWR(0x0d, ec_ioctl_slave_sdo_download_t)
#define EC_IOCTL_SLAVE_SII_READ       EC_IOWR(0x0e, ec_ioctl_slave_sii_t)
#define EC_IOCTL_SLAVE_SII_WRITE       EC_IOW(0x0f, ec_ioctl_slave_sii_t)
#define EC_IOCTL_SLAVE_REG_READ       EC_IOWR(0x10, ec_ioctl_slave_reg_t)
#define EC_IOCTL_SLAVE_REG_WRITE       EC_IOW(0x11, ec_ioctl_slave_reg_t)
#define EC_IOCTL_SLAVE_FOE_READ       EC_IOWR(0x12, ec_ioctl_slave_foe_t)
#define EC_IOCTL_SLAVE_FOE_WRITE       EC_IOW(0x13, ec_ioctl_slave_foe_t)
#define EC_IOCTL_CONFIG               EC_IOWR(0x14, ec_ioctl_config_t)
#define EC_IOCTL_CONFIG_PDO           EC_IOWR(0x15, ec_ioctl_config_pdo_t)
#define EC_IOCTL_CONFIG_PDO_ENTRY     EC_IOWR(0x16, ec_ioctl_config_pdo_entry_t)
#define EC_IOCTL_CONFIG_SDO           EC_IOWR(0x17, ec_ioctl_config_sdo_t)
#define EC_IOCTL_EOE_HANDLER          EC_IOWR(0x18, ec_ioctl_eoe_handler_t)

// Application interface
#define EC_IOCTL_REQUEST                EC_IO(0x19)
#define EC_IOCTL_CREATE_DOMAIN          EC_IO(0x1a)
#define EC_IOCTL_CREATE_SLAVE_CONFIG  EC_IOWR(0x1b, ec_ioctl_config_t)
#define EC_IOCTL_ACTIVATE              EC_IOR(0x1c, size_t)
#define EC_IOCTL_SEND                   EC_IO(0x1d)
#define EC_IOCTL_RECEIVE                EC_IO(0x1e)
#define EC_IOCTL_MASTER_STATE          EC_IOR(0x1f, ec_master_state_t)
#define EC_IOCTL_APP_TIME              EC_IOW(0x20, ec_ioctl_app_time_t)
#define EC_IOCTL_SYNC_REF               EC_IO(0x21)
#define EC_IOCTL_SYNC_SLAVES            EC_IO(0x22)
#define EC_IOCTL_SC_SYNC               EC_IOW(0x23, ec_ioctl_config_t)
#define EC_IOCTL_SC_ADD_PDO            EC_IOW(0x24, ec_ioctl_config_pdo_t)
#define EC_IOCTL_SC_CLEAR_PDOS         EC_IOW(0x25, ec_ioctl_config_pdo_t)
#define EC_IOCTL_SC_ADD_ENTRY          EC_IOW(0x26, ec_ioctl_add_pdo_entry_t)
#define EC_IOCTL_SC_CLEAR_ENTRIES      EC_IOW(0x27, ec_ioctl_config_pdo_t)
#define EC_IOCTL_SC_REG_PDO_ENTRY     EC_IOWR(0x28, ec_ioctl_reg_pdo_entry_t)
#define EC_IOCTL_SC_DC                 EC_IOW(0x29, ec_ioctl_config_t)
#define EC_IOCTL_SC_SDO                EC_IOW(0x2a, ec_ioctl_sc_sdo_t)
#define EC_IOCTL_SC_SDO_REQUEST       EC_IOWR(0x2b, ec_ioctl_sdo_request_t)
#define EC_IOCTL_SC_VOE               EC_IOWR(0x2c, ec_ioctl_voe_t)
#define EC_IOCTL_SC_STATE             EC_IOWR(0x2d, ec_ioctl_sc_state_t)
#define EC_IOCTL_DOMAIN_OFFSET          EC_IO(0x2e)
#define EC_IOCTL_DOMAIN_PROCESS         EC_IO(0x2f)
#define EC_IOCTL_DOMAIN_QUEUE           EC_IO(0x30)
#define EC_IOCTL_DOMAIN_STATE         EC_IOWR(0x31, ec_ioctl_domain_state_t)
#define EC_IOCTL_SDO_REQUEST_TIMEOUT  EC_IOWR(0x32, ec_ioctl_sdo_request_t)
#define EC_IOCTL_SDO_REQUEST_STATE    EC_IOWR(0x33, ec_ioctl_sdo_request_t)
#define EC_IOCTL_SDO_REQUEST_READ     EC_IOWR(0x34, ec_ioctl_sdo_request_t)
#define EC_IOCTL_SDO_REQUEST_WRITE    EC_IOWR(0x35, ec_ioctl_sdo_request_t)
#define EC_IOCTL_SDO_REQUEST_DATA     EC_IOWR(0x36, ec_ioctl_sdo_request_t)
#define EC_IOCTL_VOE_SEND_HEADER       EC_IOW(0x37, ec_ioctl_voe_t)
#define EC_IOCTL_VOE_REC_HEADER       EC_IOWR(0x38, ec_ioctl_voe_t)
#define EC_IOCTL_VOE_READ              EC_IOW(0x39, ec_ioctl_voe_t)
#define EC_IOCTL_VOE_READ_NOSYNC       EC_IOW(0x3a, ec_ioctl_voe_t)
#define EC_IOCTL_VOE_WRITE            EC_IOWR(0x3b, ec_ioctl_voe_t)
#define EC_IOCTL_VOE_EXEC             EC_IOWR(0x3c, ec_ioctl_voe_t)
#define EC_IOCTL_VOE_DATA             EC_IOWR(0x3d, ec_ioctl_voe_t)

/*****************************************************************************/

#define EC_IOCTL_STRING_SIZE 64

/*****************************************************************************/

typedef struct {
    uint32_t slave_count;
    uint32_t config_count;
    uint32_t domain_count;
    uint32_t eoe_handler_count;
    uint8_t phase;
    uint8_t scan_busy;
    struct {
        uint8_t address[6];
        uint8_t attached;
        uint8_t link_state;
        uint32_t tx_count;
        uint32_t rx_count;
    } devices[2];
    uint64_t app_time;
    uint16_t ref_clock;
} ec_ioctl_master_t;

/*****************************************************************************/

typedef struct {
    // input
    uint16_t position;

    // outputs
    uint32_t vendor_id;
    uint32_t product_code;
    uint32_t revision_number;
    uint32_t serial_number;
    uint16_t alias;
    uint16_t boot_rx_mailbox_offset;
    uint16_t boot_rx_mailbox_size;
    uint16_t boot_tx_mailbox_offset;
    uint16_t boot_tx_mailbox_size;
    uint16_t std_rx_mailbox_offset;
    uint16_t std_rx_mailbox_size;
    uint16_t std_tx_mailbox_offset;
    uint16_t std_tx_mailbox_size;
    uint16_t mailbox_protocols;
    uint8_t has_general_category;
    ec_sii_coe_details_t coe_details;
    ec_sii_general_flags_t general_flags;
    int16_t current_on_ebus;
    struct {
        ec_slave_port_desc_t desc;
        ec_slave_port_link_t link;
        uint32_t receive_time;
        uint16_t next_slave;
        uint32_t delay_to_next_dc;
    } ports[EC_MAX_PORTS];
    uint8_t fmmu_bit;
    uint8_t dc_supported;
    ec_slave_dc_range_t dc_range;
    uint8_t has_dc_system_time;
    uint32_t transmission_delay;
    uint8_t al_state;
    uint8_t error_flag;
    uint8_t sync_count;
    uint16_t sdo_count;
    uint32_t sii_nwords;
    char group[EC_IOCTL_STRING_SIZE];
    char image[EC_IOCTL_STRING_SIZE];
    char order[EC_IOCTL_STRING_SIZE];
    char name[EC_IOCTL_STRING_SIZE];
} ec_ioctl_slave_t;

/*****************************************************************************/

typedef struct {
    // inputs
    uint16_t slave_position;
    uint32_t sync_index;

    // outputs
    uint16_t physical_start_address;
    uint16_t default_size;
    uint8_t control_register;
    uint8_t enable;
    uint8_t pdo_count;
} ec_ioctl_slave_sync_t;

/*****************************************************************************/

typedef struct {
    // inputs
    uint16_t slave_position;
    uint32_t sync_index;
    uint32_t pdo_pos;

    // outputs
    uint16_t index;
    uint8_t entry_count;
    int8_t name[EC_IOCTL_STRING_SIZE];
} ec_ioctl_slave_sync_pdo_t;

/*****************************************************************************/

typedef struct {
    // inputs
    uint16_t slave_position;
    uint32_t sync_index;
    uint32_t pdo_pos;
    uint32_t entry_pos;

    // outputs
    uint16_t index;
    uint8_t subindex;
    uint8_t bit_length;
    int8_t name[EC_IOCTL_STRING_SIZE];
} ec_ioctl_slave_sync_pdo_entry_t;

/*****************************************************************************/

typedef struct {
    // inputs
	uint32_t index;

    // outputs
	uint32_t data_size;
	uint32_t logical_base_address;
	uint16_t working_counter;
	uint16_t expected_working_counter;
    uint32_t fmmu_count;
} ec_ioctl_domain_t;

/*****************************************************************************/

typedef struct {
    // inputs
	uint32_t domain_index;
	uint32_t fmmu_index;

    // outputs
    uint16_t slave_config_alias;
    uint16_t slave_config_position;
    uint8_t sync_index;
    ec_direction_t dir;
	uint32_t logical_address;
    uint32_t data_size;
} ec_ioctl_domain_fmmu_t;

/*****************************************************************************/

typedef struct {
    // inputs
	uint32_t domain_index;
    uint32_t data_size;
    uint8_t *target;
} ec_ioctl_domain_data_t;

/*****************************************************************************/

typedef struct {
    // inputs
	uint16_t slave_position;
    uint8_t al_state;
} ec_ioctl_slave_state_t;

/*****************************************************************************/

typedef struct {
    // inputs
    uint16_t slave_position;
    uint16_t sdo_position;

    // outputs
    uint16_t sdo_index;
    uint8_t max_subindex;
    int8_t name[EC_IOCTL_STRING_SIZE];
} ec_ioctl_slave_sdo_t;

/*****************************************************************************/

typedef struct {
    // inputs
    uint16_t slave_position;
    int sdo_spec; // positive: index, negative: list position
    uint8_t sdo_entry_subindex;

    // outputs
    uint16_t data_type;
    uint16_t bit_length;
    uint8_t read_access[EC_SDO_ENTRY_ACCESS_COUNT];
    uint8_t write_access[EC_SDO_ENTRY_ACCESS_COUNT];
    int8_t description[EC_IOCTL_STRING_SIZE];
} ec_ioctl_slave_sdo_entry_t;

/*****************************************************************************/

typedef struct {
    // inputs
    uint16_t slave_position;
    uint16_t sdo_index;
    uint8_t sdo_entry_subindex;
    uint32_t target_size;
    uint8_t *target;

    // outputs
    uint32_t data_size;
    uint32_t abort_code;
} ec_ioctl_slave_sdo_upload_t;

/*****************************************************************************/

typedef struct {
    // inputs
    uint16_t slave_position;
    uint16_t sdo_index;
    uint8_t sdo_entry_subindex;
    uint32_t data_size;
    uint8_t *data;

    // outputs
    uint32_t abort_code;
} ec_ioctl_slave_sdo_download_t;

/*****************************************************************************/

typedef struct {
    // inputs
    uint16_t slave_position;
    uint16_t offset;
    uint32_t nwords;
    uint16_t *words;
} ec_ioctl_slave_sii_t;

/*****************************************************************************/

typedef struct {
    // inputs
    uint16_t slave_position;
    uint16_t offset;
    uint16_t length;
    uint8_t *data;
} ec_ioctl_slave_reg_t;

/*****************************************************************************/

typedef struct {
    // inputs
    uint16_t slave_position;
    uint16_t offset;
    uint32_t buffer_size;
    uint8_t *buffer;

    // outputs
    uint32_t data_size;
    uint32_t result;
    uint32_t error_code;
    char file_name[32];
} ec_ioctl_slave_foe_t;

/*****************************************************************************/

typedef struct {
    // inputs
    uint32_t config_index;

    // outputs
    uint16_t alias;
    uint16_t position;
    uint32_t vendor_id;
    uint32_t product_code;
    struct {
        ec_direction_t dir;
        uint32_t pdo_count;
    } syncs[EC_MAX_SYNC_MANAGERS];
    uint32_t sdo_count;
    int32_t slave_position;
    uint16_t dc_assign_activate;
    ec_sync_signal_t dc_sync[EC_SYNC_SIGNAL_COUNT];
} ec_ioctl_config_t;

/*****************************************************************************/

typedef struct {
    // inputs
    uint32_t config_index;
    uint8_t sync_index;
    uint16_t pdo_pos;

    // outputs
    uint16_t index;
    uint8_t entry_count;
    int8_t name[EC_IOCTL_STRING_SIZE];
} ec_ioctl_config_pdo_t;

/*****************************************************************************/

typedef struct {
    // inputs
    uint32_t config_index;
    uint8_t sync_index;
    uint16_t pdo_pos;
    uint8_t entry_pos;

    // outputs
    uint16_t index;
    uint8_t subindex;
    uint8_t bit_length;
    int8_t name[EC_IOCTL_STRING_SIZE];
} ec_ioctl_config_pdo_entry_t;

/*****************************************************************************/

typedef struct {
    // inputs
    uint32_t config_index;
    uint32_t sdo_pos;

    // outputs
    uint16_t index;
    uint8_t subindex;
    uint32_t size;
    uint8_t data[4];
} ec_ioctl_config_sdo_t;

/*****************************************************************************/

typedef struct {
    // input
    uint16_t eoe_index;

    // outputs
	char name[EC_DATAGRAM_NAME_SIZE];
	uint16_t slave_position;
	uint8_t open;
	uint32_t rx_bytes;
	uint32_t rx_rate;
	uint32_t tx_bytes;
	uint32_t tx_rate;
	uint32_t tx_queued_frames;
	uint32_t tx_queue_size;
} ec_ioctl_eoe_handler_t;

/*****************************************************************************/

typedef struct {
    // inputs
    uint32_t config_index;
    uint16_t pdo_index;
    uint16_t entry_index;
    uint8_t entry_subindex;
    uint8_t entry_bit_length;
} ec_ioctl_add_pdo_entry_t;

/*****************************************************************************/

typedef struct {
    // inputs
    uint32_t config_index;
    uint16_t entry_index;
    uint8_t entry_subindex;
    uint32_t domain_index;
    
    // outputs
    unsigned int bit_position;
} ec_ioctl_reg_pdo_entry_t;

/*****************************************************************************/

typedef struct {
    // inputs
    uint32_t config_index;
    uint16_t index;
    uint8_t subindex;
    const uint8_t *data;
    size_t size;
} ec_ioctl_sc_sdo_t;

/*****************************************************************************/

typedef struct {
    // inputs
    uint32_t config_index;

    // outputs
    ec_slave_config_state_t *state;
} ec_ioctl_sc_state_t;

/*****************************************************************************/

typedef struct {
    // inputs
    uint32_t domain_index;

    // outputs
    ec_domain_state_t *state;
} ec_ioctl_domain_state_t;

/*****************************************************************************/

typedef struct {
    // inputs
    uint32_t config_index;

    // inputs/outputs
    uint32_t request_index;
    uint16_t sdo_index;
    uint8_t sdo_subindex;
    size_t size;
    uint8_t *data;
    uint32_t timeout;
    ec_request_state_t state;
} ec_ioctl_sdo_request_t;

/*****************************************************************************/

typedef struct {
    // inputs
    uint32_t config_index;

    // inputs/outputs
    uint32_t voe_index;
    uint32_t *vendor_id;
    uint16_t *vendor_type;
    size_t size;
    uint8_t *data;
    ec_request_state_t state;
} ec_ioctl_voe_t;

/*****************************************************************************/

typedef struct {
    // inputs
    uint64_t app_time;
} ec_ioctl_app_time_t;

/*****************************************************************************/

/** \endcond */

#endif
