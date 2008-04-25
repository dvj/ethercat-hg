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

#include "globals.h"
#include "master.h"
#include "mailbox.h"
#include "slave_config.h"
#ifdef EC_EOE
#include "ethernet.h"
#endif

#include "fsm_master.h"

/*****************************************************************************/

void ec_fsm_master_state_start(ec_fsm_master_t *);
void ec_fsm_master_state_broadcast(ec_fsm_master_t *);
void ec_fsm_master_state_read_states(ec_fsm_master_t *);
void ec_fsm_master_state_acknowledge(ec_fsm_master_t *);
void ec_fsm_master_state_configure_slave(ec_fsm_master_t *);
void ec_fsm_master_state_clear_addresses(ec_fsm_master_t *);
void ec_fsm_master_state_scan_slaves(ec_fsm_master_t *);
void ec_fsm_master_state_write_sii(ec_fsm_master_t *);
void ec_fsm_master_state_sdodict(ec_fsm_master_t *);
void ec_fsm_master_state_sdo_request(ec_fsm_master_t *);
void ec_fsm_master_state_end(ec_fsm_master_t *);
void ec_fsm_master_state_error(ec_fsm_master_t *);

/*****************************************************************************/

/**
   Constructor.
*/

void ec_fsm_master_init(ec_fsm_master_t *fsm, /**< master state machine */
        ec_master_t *master, /**< EtherCAT master */
        ec_datagram_t *datagram /**< datagram object to use */
        )
{
    fsm->master = master;
    fsm->datagram = datagram;
    fsm->state = ec_fsm_master_state_start;
    fsm->idle = 0;
    fsm->slaves_responding = 0;
    fsm->topology_change_pending = 0;
    fsm->slave_states = EC_SLAVE_STATE_UNKNOWN;

    // init sub-state-machines
    ec_fsm_slave_config_init(&fsm->fsm_slave_config, fsm->datagram);
    ec_fsm_slave_scan_init(&fsm->fsm_slave_scan, fsm->datagram,
            &fsm->fsm_slave_config, &fsm->fsm_coe_map);
    ec_fsm_sii_init(&fsm->fsm_sii, fsm->datagram);
    ec_fsm_change_init(&fsm->fsm_change, fsm->datagram);
    ec_fsm_coe_init(&fsm->fsm_coe, fsm->datagram);
    ec_fsm_coe_map_init(&fsm->fsm_coe_map, &fsm->fsm_coe);
}

/*****************************************************************************/

/**
   Destructor.
*/

void ec_fsm_master_clear(ec_fsm_master_t *fsm /**< master state machine */)
{
    // clear sub-state machines
    ec_fsm_slave_config_clear(&fsm->fsm_slave_config);
    ec_fsm_slave_scan_clear(&fsm->fsm_slave_scan);
    ec_fsm_sii_clear(&fsm->fsm_sii);
    ec_fsm_change_clear(&fsm->fsm_change);
    ec_fsm_coe_clear(&fsm->fsm_coe);
    ec_fsm_coe_map_clear(&fsm->fsm_coe_map);
}

/*****************************************************************************/

/**
   Executes the current state of the state machine.
   If the state machine's datagram is not sent or received yet, the execution
   of the state machine is delayed to the next cycle.
   \return false, if state machine has terminated
*/

int ec_fsm_master_exec(ec_fsm_master_t *fsm /**< master state machine */)
{
    if (fsm->datagram->state == EC_DATAGRAM_SENT
        || fsm->datagram->state == EC_DATAGRAM_QUEUED) {
        // datagram was not sent or received yet.
        return ec_fsm_master_running(fsm);
    }

    fsm->state(fsm);
    return ec_fsm_master_running(fsm);
}

/*****************************************************************************/

/**
 * \return false, if state machine has terminated
 */

int ec_fsm_master_running(
        const ec_fsm_master_t *fsm /**< master state machine */
        )
{
    return fsm->state != ec_fsm_master_state_end
        && fsm->state != ec_fsm_master_state_error;
}

/*****************************************************************************/

/**
 * \return true, if the state machine is in an idle phase
 */

int ec_fsm_master_idle(
        const ec_fsm_master_t *fsm /**< master state machine */
        )
{
    return fsm->idle;
}

/******************************************************************************
 *  master state machine
 *****************************************************************************/

/**
   Master state: START.
   Starts with getting slave count and slave states.
*/

void ec_fsm_master_state_start(ec_fsm_master_t *fsm)
{
    fsm->idle = 1;
    ec_datagram_brd(fsm->datagram, 0x0130, 2);
    fsm->state = ec_fsm_master_state_broadcast;
}

/*****************************************************************************/

/**
   Master state: BROADCAST.
   Processes the broadcast read slave count and slaves states.
*/

void ec_fsm_master_state_broadcast(ec_fsm_master_t *fsm /**< master state machine */)
{
    ec_datagram_t *datagram = fsm->datagram;
    unsigned int i;
    ec_slave_t *slave;
    ec_master_t *master = fsm->master;

    if (datagram->state == EC_DATAGRAM_TIMED_OUT)
        return; // always retry

    if (datagram->state != EC_DATAGRAM_RECEIVED) { // link is down
        fsm->slaves_responding = 0;
        list_for_each_entry(slave, &master->slaves, list) {
            ec_slave_set_online_state(slave, EC_SLAVE_OFFLINE);
        }
        fsm->state = ec_fsm_master_state_error;
        return;
    }

    // bus topology change?
    if (datagram->working_counter != fsm->slaves_responding) {
        fsm->topology_change_pending = 1;
        fsm->slaves_responding = datagram->working_counter;

        EC_INFO("%u slave%s responding.\n",
                fsm->slaves_responding,
                fsm->slaves_responding == 1 ? "" : "s");
    }

    // slave states changed?
    if (EC_READ_U8(datagram->data) != fsm->slave_states) {
        char states[EC_STATE_STRING_SIZE];
        fsm->slave_states = EC_READ_U8(datagram->data);
        ec_state_string(fsm->slave_states, states);
        EC_INFO("Slave states: %s.\n", states);
    }

    if (fsm->topology_change_pending) {
        down(&master->scan_sem);
        if (!master->allow_scan) {
            up(&master->scan_sem);
        } else {
            master->scan_busy = 1;
            up(&master->scan_sem);
            
            // topology change when scan is allowed:
            // clear all slaves and scan the bus
            fsm->topology_change_pending = 0;
            fsm->idle = 0;
            fsm->scan_jiffies = jiffies;

#ifdef EC_EOE
            ec_master_eoe_stop(master);
            ec_master_clear_eoe_handlers(master);
#endif
            ec_master_destroy_slaves(master);

            master->slave_count = datagram->working_counter;

            if (!master->slave_count) {
                // no slaves present -> finish state machine.
                master->scan_busy = 0;
                wake_up_interruptible(&master->scan_queue);
                fsm->state = ec_fsm_master_state_end;
                return;
            }

            // init slaves
            for (i = 0; i < master->slave_count; i++) {
                if (!(slave = (ec_slave_t *) kmalloc(sizeof(ec_slave_t),
                                GFP_ATOMIC))) {
                    EC_ERR("Failed to allocate slave %i!\n", i);
                    ec_master_destroy_slaves(master);
                    master->scan_busy = 0;
                    wake_up_interruptible(&master->scan_queue);
                    fsm->state = ec_fsm_master_state_error;
                    return;
                }

                if (ec_slave_init(slave, master, i, i + 1)) {
                    // freeing of "slave" already done
                    ec_master_destroy_slaves(master);
                    master->scan_busy = 0;
                    wake_up_interruptible(&master->scan_queue);
                    fsm->state = ec_fsm_master_state_error;
                    return;
                }

                list_add_tail(&slave->list, &master->slaves);
            }

            if (master->debug_level)
                EC_DBG("Clearing station addresses...\n");

            ec_datagram_bwr(datagram, 0x0010, 2);
            EC_WRITE_U16(datagram->data, 0x0000);
            fsm->retries = EC_FSM_RETRIES;
            fsm->state = ec_fsm_master_state_clear_addresses;
            return;
        }
    }

    if (list_empty(&master->slaves)) {
        fsm->state = ec_fsm_master_state_end;
    } else {
        // fetch state from each slave
        fsm->slave = list_entry(master->slaves.next, ec_slave_t, list);
        ec_datagram_fprd(fsm->datagram, fsm->slave->station_address, 0x0130, 2);
        fsm->retries = EC_FSM_RETRIES;
        fsm->state = ec_fsm_master_state_read_states;
    }
}

/*****************************************************************************/

/**
 * Check for pending SII write requests and process one.
 * \return non-zero, if an SII write request is processed.
 */

int ec_fsm_master_action_process_sii(
        ec_fsm_master_t *fsm /**< master state machine */
        )
{
    ec_master_t *master = fsm->master;
    ec_sii_write_request_t *request;
    ec_slave_t *slave;

    // search the first request to be processed
    while (1) {
        down(&master->sii_sem);
        if (list_empty(&master->sii_requests)) {
            up(&master->sii_sem);
            break;
        }
        // get first request
        request = list_entry(master->sii_requests.next,
                ec_sii_write_request_t, list);
        list_del_init(&request->list); // dequeue
        request->state = EC_REQUEST_BUSY;
        up(&master->sii_sem);

        slave = request->slave;
        if (slave->online_state == EC_SLAVE_OFFLINE) {
            EC_ERR("Discarding SII data, slave %i offline.\n",
                    slave->ring_position);
            request->state = EC_REQUEST_FAILURE;
            wake_up(&master->sii_queue);
            continue;
        }

        // found pending SII write operation. execute it!
        if (master->debug_level)
            EC_DBG("Writing SII data to slave %i...\n",
                    slave->ring_position);
        fsm->sii_request = request;
        fsm->sii_index = 0;
        ec_fsm_sii_write(&fsm->fsm_sii, request->slave, request->word_offset,
                request->data, EC_FSM_SII_USE_CONFIGURED_ADDRESS);
        fsm->state = ec_fsm_master_state_write_sii;
        fsm->state(fsm); // execute immediately
        return 1;
    }

    return 0;
}

/*****************************************************************************/

/**
 * Check for pending Sdo requests and process one.
 * \return non-zero, if an Sdo request is processed.
 */

int ec_fsm_master_action_process_sdo(
        ec_fsm_master_t *fsm /**< master state machine */
        )
{
    ec_master_t *master = fsm->master;
    ec_master_sdo_request_t *request;
    ec_sdo_request_t *req;
    ec_slave_t *slave;

    // search for internal requests to be processed
    list_for_each_entry(slave, &master->slaves, list) {
        if (!slave->config)
            continue;
        list_for_each_entry(req, &slave->config->sdo_requests, list) {
            if (req->state == EC_REQUEST_QUEUED) {

                if (ec_sdo_request_timed_out(req)) {
                    req->state = EC_REQUEST_FAILURE;
                    if (master->debug_level)
                        EC_DBG("Sdo request for slave %u timed out...\n",
                                slave->ring_position);
                    continue;
                }

                if (slave->current_state == EC_SLAVE_STATE_INIT ||
                        slave->error_flag) {
                    req->state = EC_REQUEST_FAILURE;
                    continue;
                }

                req->state = EC_REQUEST_BUSY;
                if (master->debug_level)
                    EC_DBG("Processing Sdo request for slave %u...\n",
                            slave->ring_position);

                fsm->idle = 0;
                fsm->sdo_request = req;
                fsm->slave = slave;
                fsm->state = ec_fsm_master_state_sdo_request;
                ec_fsm_coe_transfer(&fsm->fsm_coe, slave, req);
                ec_fsm_coe_exec(&fsm->fsm_coe); // execute immediately
                return 1;
            }
        }
    }
    
    // search the first external request to be processed
    while (1) {
        down(&master->sdo_sem);
        if (list_empty(&master->slave_sdo_requests)) {
            up(&master->sdo_sem);
            break;
        }
        // get first request
        request = list_entry(master->slave_sdo_requests.next,
                ec_master_sdo_request_t, list);
        list_del_init(&request->list); // dequeue
        request->req.state = EC_REQUEST_BUSY;
        up(&master->sdo_sem);

        slave = request->slave;
        if (slave->current_state == EC_SLAVE_STATE_INIT ||
                slave->online_state == EC_SLAVE_OFFLINE ||
                slave->error_flag) {
            EC_ERR("Discarding Sdo request, slave %u not ready.\n",
                    slave->ring_position);
            request->req.state = EC_REQUEST_FAILURE;
            wake_up(&master->sdo_queue);
            continue;
        }

        // Found pending Sdo request. Execute it!
        if (master->debug_level)
            EC_DBG("Processing Sdo request for slave %i...\n",
                    slave->ring_position);

        // Start uploading Sdo
        fsm->idle = 0;
        fsm->sdo_request = &request->req;
        fsm->slave = slave;
        fsm->state = ec_fsm_master_state_sdo_request;
        ec_fsm_coe_transfer(&fsm->fsm_coe, slave, &request->req);
        ec_fsm_coe_exec(&fsm->fsm_coe); // execute immediately
        return 1;
    }

    return 0;
}

/*****************************************************************************/

/**
 * Check for slaves that are not configured and configure them.
 */

int ec_fsm_master_action_configure(
        ec_fsm_master_t *fsm /**< master state machine */
        )
{
    ec_slave_t *slave;
    ec_master_t *master = fsm->master;
    char old_state[EC_STATE_STRING_SIZE], new_state[EC_STATE_STRING_SIZE];

    // check if any slaves are not in the state, they're supposed to be
    // FIXME do not check all slaves in every cycle...
    list_for_each_entry(slave, &master->slaves, list) {
        if (slave->error_flag
                || slave->online_state == EC_SLAVE_OFFLINE
                || slave->requested_state == EC_SLAVE_STATE_UNKNOWN
                || (slave->current_state == slave->requested_state
                    && slave->self_configured)) continue;

        if (master->debug_level) {
            ec_state_string(slave->current_state, old_state);
            if (slave->current_state != slave->requested_state) {
                ec_state_string(slave->requested_state, new_state);
                EC_DBG("Changing state of slave %i (%s -> %s).\n",
                        slave->ring_position, old_state, new_state);
            }
            else if (!slave->self_configured) {
                EC_DBG("Reconfiguring slave %i (%s).\n",
                        slave->ring_position, old_state);
            }
        }

        fsm->idle = 0;
        fsm->slave = slave;
        fsm->state = ec_fsm_master_state_configure_slave;
        ec_fsm_slave_config_start(&fsm->fsm_slave_config, slave);
        ec_fsm_slave_config_exec(&fsm->fsm_slave_config); // execute immediately
        return 1;
    }

    master->config_busy = 0;
    wake_up_interruptible(&master->config_queue);
    return 0;
}

/*****************************************************************************/

/**
   Master action: PROC_STATES.
   Processes the slave states.
*/

void ec_fsm_master_action_process_states(ec_fsm_master_t *fsm
                                         /**< master state machine */
                                         )
{
    ec_master_t *master = fsm->master;
    ec_slave_t *slave;

    // Start slave configuration, if it is allowed.
    down(&master->config_sem);
    if (!master->allow_config) {
        up(&master->config_sem);
    } else {
        master->config_busy = 1;
        up(&master->config_sem);

        // check for pending slave configurations
        if (ec_fsm_master_action_configure(fsm))
            return;
    }

    // Check for pending Sdo requests
    if (ec_fsm_master_action_process_sdo(fsm))
        return;

    if (master->mode == EC_MASTER_MODE_IDLE) {

        // check, if slaves have an Sdo dictionary to read out.
        list_for_each_entry(slave, &master->slaves, list) {
            if (!(slave->sii.mailbox_protocols & EC_MBOX_COE)
                || slave->sdo_dictionary_fetched
                || slave->current_state == EC_SLAVE_STATE_INIT
                || jiffies - slave->jiffies_preop < EC_WAIT_SDO_DICT * HZ
                || slave->online_state == EC_SLAVE_OFFLINE
                || slave->error_flag) continue;

            if (master->debug_level) {
                EC_DBG("Fetching Sdo dictionary from slave %i.\n",
                       slave->ring_position);
            }

            slave->sdo_dictionary_fetched = 1;

            // start fetching Sdo dictionary
            fsm->idle = 0;
            fsm->slave = slave;
            fsm->state = ec_fsm_master_state_sdodict;
            ec_fsm_coe_dictionary(&fsm->fsm_coe, slave);
            ec_fsm_coe_exec(&fsm->fsm_coe); // execute immediately
            return;
        }

        // check for pending SII write operations.
        if (ec_fsm_master_action_process_sii(fsm))
            return; // SII write request found
    }

    fsm->state = ec_fsm_master_state_end;
}

/*****************************************************************************/

/**
   Master action: Get state of next slave.
*/

void ec_fsm_master_action_next_slave_state(ec_fsm_master_t *fsm
                                           /**< master state machine */)
{
    ec_master_t *master = fsm->master;
    ec_slave_t *slave = fsm->slave;

    // is there another slave to query?
    if (slave->list.next != &master->slaves) {
        // process next slave
        fsm->idle = 1;
        fsm->slave = list_entry(slave->list.next, ec_slave_t, list);
        ec_datagram_fprd(fsm->datagram, fsm->slave->station_address,
                         0x0130, 2);
        fsm->retries = EC_FSM_RETRIES;
        fsm->state = ec_fsm_master_state_read_states;
        return;
    }

    // all slave states read
    ec_fsm_master_action_process_states(fsm);
}

/*****************************************************************************/

/**
   Master state: READ STATES.
   Fetches the AL- and online state of a slave.
*/

void ec_fsm_master_state_read_states(ec_fsm_master_t *fsm /**< master state machine */)
{
    ec_slave_t *slave = fsm->slave;
    ec_datagram_t *datagram = fsm->datagram;

    if (datagram->state == EC_DATAGRAM_TIMED_OUT && fsm->retries--)
        return;

    if (datagram->state != EC_DATAGRAM_RECEIVED) {
        EC_ERR("Failed to receive AL state datagram for slave %i"
                " (datagram state %i)\n",
                slave->ring_position, datagram->state);
        fsm->state = ec_fsm_master_state_error;
        return;
    }

    // did the slave not respond to its station address?
    if (datagram->working_counter == 0) {
        ec_slave_set_online_state(slave, EC_SLAVE_OFFLINE);
        ec_fsm_master_action_next_slave_state(fsm);
        return;
    }

    // FIXME what to to on multiple response?

    // slave responded
    ec_slave_set_state(slave, EC_READ_U8(datagram->data)); // set app state first
    ec_slave_set_online_state(slave, EC_SLAVE_ONLINE);

    // check, if new slave state has to be acknowledged
    if (slave->current_state & EC_SLAVE_STATE_ACK_ERR && !slave->error_flag) {
        fsm->idle = 0;
        fsm->state = ec_fsm_master_state_acknowledge;
        ec_fsm_change_ack(&fsm->fsm_change, slave);
        ec_fsm_change_exec(&fsm->fsm_change);
        return;
    }

    ec_fsm_master_action_next_slave_state(fsm);
}

/*****************************************************************************/

/**
   Master state: ACKNOWLEDGE
*/

void ec_fsm_master_state_acknowledge(ec_fsm_master_t *fsm /**< master state machine */)
{
    ec_slave_t *slave = fsm->slave;

    if (ec_fsm_change_exec(&fsm->fsm_change)) return;

    if (!ec_fsm_change_success(&fsm->fsm_change)) {
        fsm->slave->error_flag = 1;
        EC_ERR("Failed to acknowledge state change on slave %i.\n",
               slave->ring_position);
        fsm->state = ec_fsm_master_state_error;
        return;
    }

    ec_fsm_master_action_next_slave_state(fsm);
}

/*****************************************************************************/

/**
 * Master state: CLEAR ADDRESSES.
 */

void ec_fsm_master_state_clear_addresses(
        ec_fsm_master_t *fsm /**< master state machine */
        )
{
    ec_master_t *master = fsm->master;
    ec_datagram_t *datagram = fsm->datagram;

    if (datagram->state == EC_DATAGRAM_TIMED_OUT && fsm->retries--)
        return;

    if (datagram->state != EC_DATAGRAM_RECEIVED) {
        EC_ERR("Failed to receive address clearing datagram (state %i).\n",
                datagram->state);
        master->scan_busy = 0;
        wake_up_interruptible(&master->scan_queue);
        fsm->state = ec_fsm_master_state_error;
        return;
    }

    if (datagram->working_counter != master->slave_count) {
        EC_WARN("Failed to clear all station addresses: Cleared %u of %u",
                datagram->working_counter, master->slave_count);
    }

    EC_INFO("Scanning bus.\n");

    // begin scanning of slaves
    fsm->slave = list_entry(master->slaves.next, ec_slave_t, list);
    fsm->state = ec_fsm_master_state_scan_slaves;
    ec_fsm_slave_scan_start(&fsm->fsm_slave_scan, fsm->slave);
    ec_fsm_slave_scan_exec(&fsm->fsm_slave_scan); // execute immediately
}

/*****************************************************************************/

/**
 * Master state: SCAN SLAVES.
 * Executes the sub-statemachine for the scanning of a slave.
 */

void ec_fsm_master_state_scan_slaves(
        ec_fsm_master_t *fsm /**< master state machine */
        )
{
    ec_master_t *master = fsm->master;
    ec_slave_t *slave = fsm->slave;

    if (ec_fsm_slave_scan_exec(&fsm->fsm_slave_scan)) // execute slave state machine
        return;

#ifdef EC_EOE
    if (slave->sii.mailbox_protocols & EC_MBOX_EOE) {
        // create EoE handler for this slave
        ec_eoe_t *eoe;
        if (!(eoe = kmalloc(sizeof(ec_eoe_t), GFP_KERNEL))) {
            EC_ERR("Failed to allocate EoE handler memory for slave %u!\n",
                    slave->ring_position);
        } else if (ec_eoe_init(eoe, slave)) {
            EC_ERR("Failed to init EoE handler for slave %u!\n",
                    slave->ring_position);
            kfree(eoe);
        } else {
            list_add_tail(&eoe->list, &master->eoe_handlers);
        }
    }
#endif

    // another slave to fetch?
    if (slave->list.next != &master->slaves) {
        fsm->slave = list_entry(slave->list.next, ec_slave_t, list);
        ec_fsm_slave_scan_start(&fsm->fsm_slave_scan, fsm->slave);
        ec_fsm_slave_scan_exec(&fsm->fsm_slave_scan); // execute immediately
        return;
    }

    EC_INFO("Bus scanning completed in %u ms.\n",
            (u32) (jiffies - fsm->scan_jiffies) * 1000 / HZ);

    master->scan_busy = 0;
    wake_up_interruptible(&master->scan_queue);

    // Attach slave configurations
    ec_master_attach_slave_configs(master);

#ifdef EC_EOE
    // check if EoE processing has to be started
    ec_master_eoe_start(master);
#endif

    fsm->state = ec_fsm_master_state_end;
}

/*****************************************************************************/

/**
   Master state: CONFIGURE SLAVES.
   Starts configuring a slave.
*/

void ec_fsm_master_state_configure_slave(ec_fsm_master_t *fsm
                                   /**< master state machine */
                                   )
{
    if (ec_fsm_slave_config_exec(&fsm->fsm_slave_config)) // execute slave's state machine
        return;

    if (!ec_fsm_slave_config_success(&fsm->fsm_slave_config)) {
        // TODO: mark slave_config as failed.
    }

    // configure next slave, if necessary
    if (ec_fsm_master_action_configure(fsm))
        return;

    fsm->state = ec_fsm_master_state_end;
}

/*****************************************************************************/

/**
   Master state: WRITE SII.
*/

void ec_fsm_master_state_write_sii(
        ec_fsm_master_t *fsm /**< master state machine */)
{
    ec_master_t *master = fsm->master;
    ec_sii_write_request_t *request = fsm->sii_request;
    ec_slave_t *slave = request->slave;

    if (ec_fsm_sii_exec(&fsm->fsm_sii)) return;

    if (!ec_fsm_sii_success(&fsm->fsm_sii)) {
        slave->error_flag = 1;
        EC_ERR("Failed to write SII data to slave %i.\n",
                slave->ring_position);
        request->state = EC_REQUEST_FAILURE;
        wake_up(&master->sii_queue);
        fsm->state = ec_fsm_master_state_error;
        return;
    }

    fsm->sii_index++;
    if (fsm->sii_index < request->word_size) {
        ec_fsm_sii_write(&fsm->fsm_sii, slave,
                request->word_offset + fsm->sii_index,
                request->data + fsm->sii_index * 2,
                EC_FSM_SII_USE_CONFIGURED_ADDRESS);
        ec_fsm_sii_exec(&fsm->fsm_sii); // execute immediately
        return;
    }

    // finished writing SII
    if (master->debug_level)
        EC_DBG("Finished writing %u words of SII data to slave %u.\n",
                request->word_size, slave->ring_position);
    request->state = EC_REQUEST_SUCCESS;
    wake_up(&master->sii_queue);

    // TODO: Evaluate new SII contents!

    // check for another SII write request
    if (ec_fsm_master_action_process_sii(fsm))
        return; // processing another request

    fsm->state = ec_fsm_master_state_end;
}

/*****************************************************************************/

/**
   Master state: SdoDICT.
*/

void ec_fsm_master_state_sdodict(ec_fsm_master_t *fsm /**< master state machine */)
{
    ec_slave_t *slave = fsm->slave;
    ec_master_t *master = fsm->master;

    if (ec_fsm_coe_exec(&fsm->fsm_coe)) return;

    if (!ec_fsm_coe_success(&fsm->fsm_coe)) {
        fsm->state = ec_fsm_master_state_error;
        return;
    }

    // Sdo dictionary fetching finished

    if (master->debug_level) {
        unsigned int sdo_count, entry_count;
        ec_slave_sdo_dict_info(slave, &sdo_count, &entry_count);
        EC_DBG("Fetched %i Sdos and %i entries from slave %i.\n",
               sdo_count, entry_count, slave->ring_position);
    }

    fsm->state = ec_fsm_master_state_end;
}

/*****************************************************************************/

/**
   Master state: SDO REQUEST.
*/

void ec_fsm_master_state_sdo_request(ec_fsm_master_t *fsm /**< master state machine */)
{
    ec_master_t *master = fsm->master;
    ec_sdo_request_t *request = fsm->sdo_request;

    if (ec_fsm_coe_exec(&fsm->fsm_coe)) return;

    if (!ec_fsm_coe_success(&fsm->fsm_coe)) {
        EC_DBG("Failed to process Sdo request for slave %u.\n",
                fsm->slave->ring_position);
        request->state = EC_REQUEST_FAILURE;
        wake_up(&master->sdo_queue);
        fsm->state = ec_fsm_master_state_error;
        return;
    }

    // Sdo request finished 
    request->state = EC_REQUEST_SUCCESS;
    wake_up(&master->sdo_queue);

    if (master->debug_level)
        EC_DBG("Finished Sdo request for slave %u.\n",
                fsm->slave->ring_position);

    // check for another Sdo request
    if (ec_fsm_master_action_process_sdo(fsm))
        return; // processing another request

    fsm->state = ec_fsm_master_state_end;
}

/*****************************************************************************/

/**
   State: ERROR.
*/

void ec_fsm_master_state_error(
        ec_fsm_master_t *fsm /**< master state machine */
        )
{
    fsm->state = ec_fsm_master_state_start;
}

/*****************************************************************************/

/**
   State: END.
*/

void ec_fsm_master_state_end(ec_fsm_master_t *fsm /**< master state machine */)
{
    fsm->state = ec_fsm_master_state_start;
}

/*****************************************************************************/

