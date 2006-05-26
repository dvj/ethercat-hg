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
   EtherCAT finite state machines.
*/

/*****************************************************************************/

#ifndef __EC_STATES__
#define __EC_STATES__

#include "../include/ecrt.h"
#include "command.h"
#include "slave.h"

/*****************************************************************************/

typedef struct ec_fsm ec_fsm_t;

/*****************************************************************************/

/**
   Finite state machine of an EtherCAT master.
*/

struct ec_fsm
{
    ec_master_t *master; /**< master the FSM runs on */
    ec_slave_t *slave; /**< slave the FSM runs on */
    ec_command_t command; /**< command used in the state machine */

    void (*master_state)(ec_fsm_t *); /**< master state function */
    unsigned int master_slaves_responding; /**< number of responding slaves */
    ec_slave_state_t master_slave_states; /**< states of responding slaves */
    unsigned int master_validation; /**< non-zero, if validation to do */

    void (*slave_state)(ec_fsm_t *); /**< slave state function */
    uint8_t slave_sii_num; /**< SII value iteration counter */
    uint8_t *slave_cat_data; /**< temporary memory for category data */
    uint16_t slave_cat_offset; /**< current category word offset in EEPROM */
    uint16_t slave_cat_data_offset; /**< current offset in category data */
    uint16_t slave_cat_type; /**< type of current category */
    uint16_t slave_cat_words; /**< number of words of current category */

    void (*sii_state)(ec_fsm_t *); /**< SII state function */
    uint16_t sii_offset; /**< input: offset in SII */
    unsigned int sii_mode; /**< SII reading done by APRD (0) or NPRD (1) */
    uint32_t sii_result; /**< output: read SII value (32bit) */
    cycles_t sii_start; /**< sii start */

    void (*change_state)(ec_fsm_t *); /**< slave state change state function */
    uint8_t change_new; /**< input: new state */
    cycles_t change_start; /**< change start */
};

/*****************************************************************************/

int ec_fsm_init(ec_fsm_t *, ec_master_t *);
void ec_fsm_clear(ec_fsm_t *);
void ec_fsm_reset(ec_fsm_t *);
void ec_fsm_execute(ec_fsm_t *);

/*****************************************************************************/

#endif
