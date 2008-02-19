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
   EtherCAT slave configuration structure.
*/

/*****************************************************************************/

#ifndef _EC_SLAVE_CONFIG_H_
#define _EC_SLAVE_CONFIG_H_

#include <linux/list.h>
#include <linux/kobject.h>

#include "../include/ecrt.h"

#include "globals.h"
#include "slave.h"
#include "fmmu_config.h"
#include "pdo_mapping.h"

/*****************************************************************************/

/** EtherCAT slave configuration.
 */
struct ec_slave_config {
    struct list_head list; /**< List item. */
    struct kobject kobj; /**< kobject. */
    ec_master_t *master; /**< Master owning the slave configuration. */

    uint16_t alias; /**< Slave alias. */
    uint16_t position; /**< Index after alias. If alias is zero, this is the
                         ring position. */
    uint32_t vendor_id; /**< Slave vendor ID. */
    uint32_t product_code; /**< Slave product code. */

    ec_slave_t *slave; /**< Slave pointer. This is \a NULL, if the slave is
                         offline. */

    ec_pdo_mapping_t mapping[2]; /**< Output and input PDO mapping. */

    struct list_head sdo_configs; /**< SDO configurations. */

    ec_fmmu_config_t fmmu_configs[EC_MAX_FMMUS]; /**< FMMU configurations. */
    uint8_t used_fmmus; /**< Number of FMMUs used. */
};

/*****************************************************************************/

int ec_slave_config_init(ec_slave_config_t *, ec_master_t *, uint16_t,
        uint16_t, uint32_t, uint32_t);
void ec_slave_config_destroy(ec_slave_config_t *);

int ec_slave_config_reg_pdo_entry(ec_slave_config_t *, ec_domain_t *,
        uint16_t, uint8_t);

int ec_slave_config_attach(ec_slave_config_t *);
void ec_slave_config_detach(ec_slave_config_t *);

void ec_slave_config_load_default_mapping(ec_slave_config_t *);

/*****************************************************************************/

#endif
