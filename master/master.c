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
   EtherCAT master methods.
*/

/*****************************************************************************/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/delay.h>

#include "../include/ecrt.h"
#include "globals.h"
#include "master.h"
#include "slave.h"
#include "device.h"
#include "datagram.h"
#include "ethernet.h"

/*****************************************************************************/

void ec_master_destroy_domains(ec_master_t *);
void ec_master_sync_io(ec_master_t *);
static int ec_master_idle_thread(ec_master_t *);
static int ec_master_operation_thread(ec_master_t *);
void ec_master_eoe_run(unsigned long);
void ec_master_check_sdo(unsigned long);
int ec_master_measure_bus_time(ec_master_t *);
ssize_t ec_show_master_attribute(struct kobject *, struct attribute *, char *);
ssize_t ec_store_master_attribute(struct kobject *, struct attribute *,
                                  const char *, size_t);

/*****************************************************************************/

/** \cond */

EC_SYSFS_READ_ATTR(info);
EC_SYSFS_READ_WRITE_ATTR(debug_level);

static struct attribute *ec_def_attrs[] = {
    &attr_info,
    &attr_debug_level,
    NULL,
};

static struct sysfs_ops ec_sysfs_ops = {
    .show = &ec_show_master_attribute,
    .store = ec_store_master_attribute
};

static struct kobj_type ktype_ec_master = {
    .release = NULL,
    .sysfs_ops = &ec_sysfs_ops,
    .default_attrs = ec_def_attrs
};

/** \endcond */

/*****************************************************************************/

/**
   Master constructor.
   \return 0 in case of success, else < 0
*/

int ec_master_init(ec_master_t *master, /**< EtherCAT master */
        struct kobject *module_kobj, /**< kobject of the master module */
        unsigned int index, /**< master index */
        const uint8_t *main_mac, /**< MAC address of main device */
        const uint8_t *backup_mac /**< MAC address of backup device */
        )
{
    unsigned int i;

    master->index = index;
    master->reserved = 0;

    master->main_mac = main_mac;
    master->backup_mac = backup_mac;
    init_MUTEX(&master->device_sem);

    master->mode = EC_MASTER_MODE_ORPHANED;
    master->injection_seq_fsm = 0;
    master->injection_seq_rt = 0;

    INIT_LIST_HEAD(&master->slaves);
    master->slave_count = 0;
    
    master->scan_state = EC_REQUEST_IN_PROGRESS;
    master->allow_scan = 1;
    init_MUTEX(&master->scan_sem);
    init_waitqueue_head(&master->scan_queue);

    master->config_state = EC_REQUEST_COMPLETE;
    master->allow_config = 1;
    init_MUTEX(&master->config_sem);
    init_waitqueue_head(&master->config_queue);
    
    INIT_LIST_HEAD(&master->datagram_queue);
    master->datagram_index = 0;

    INIT_LIST_HEAD(&master->domains);
    master->debug_level = 0;

    master->stats.timeouts = 0;
    master->stats.corrupted = 0;
    master->stats.skipped = 0;
    master->stats.unmatched = 0;
    master->stats.output_jiffies = 0;

    for (i = 0; i < HZ; i++) {
        master->idle_cycle_times[i] = 0;
        master->eoe_cycle_times[i] = 0;
    }
    master->idle_cycle_time_pos = 0;
    master->eoe_cycle_time_pos = 0;

    init_timer(&master->eoe_timer);
    master->eoe_timer.function = ec_master_eoe_run;
    master->eoe_timer.data = (unsigned long) master;
    master->eoe_running = 0;
    INIT_LIST_HEAD(&master->eoe_handlers);

    master->internal_lock = SPIN_LOCK_UNLOCKED;
    master->request_cb = NULL;
    master->release_cb = NULL;
    master->cb_data = NULL;

    INIT_LIST_HEAD(&master->eeprom_requests);
    init_MUTEX(&master->eeprom_sem);
    init_waitqueue_head(&master->eeprom_queue);

    INIT_LIST_HEAD(&master->sdo_requests);
    init_MUTEX(&master->sdo_sem);
    init_waitqueue_head(&master->sdo_queue);

    // init devices
    if (ec_device_init(&master->main_device, master))
        goto out_return;

    if (ec_device_init(&master->backup_device, master))
        goto out_clear_main;

    // init state machine datagram
    ec_datagram_init(&master->fsm_datagram);
    if (ec_datagram_prealloc(&master->fsm_datagram, EC_MAX_DATA_SIZE)) {
        EC_ERR("Failed to allocate FSM datagram.\n");
        goto out_clear_backup;
    }

    // create state machine object
    ec_fsm_master_init(&master->fsm, master, &master->fsm_datagram);

    // init kobject and add it to the hierarchy
    memset(&master->kobj, 0x00, sizeof(struct kobject));
    kobject_init(&master->kobj);
    master->kobj.ktype = &ktype_ec_master;
    master->kobj.parent = module_kobj;
    
    if (kobject_set_name(&master->kobj, "master%i", index)) {
        EC_ERR("Failed to set master kobject name.\n");
        kobject_put(&master->kobj);
        goto out_clear_fsm;
    }
    
    if (kobject_add(&master->kobj)) {
        EC_ERR("Failed to add master kobject.\n");
        kobject_put(&master->kobj);
        goto out_clear_fsm;
    }

    return 0;

out_clear_fsm:
    ec_fsm_master_clear(&master->fsm);
out_clear_backup:
    ec_device_clear(&master->backup_device);
out_clear_main:
    ec_device_clear(&master->main_device);
out_return:
    return -1;
}

/*****************************************************************************/

/**
   Clear and free master.
   This method is called by the kobject,
   once there are no more references to it.
*/

void ec_master_clear(
        ec_master_t *master /**< EtherCAT master */
        )
{
    ec_master_clear_eoe_handlers(master);
    ec_master_destroy_slaves(master);
    ec_master_destroy_domains(master);
    ec_fsm_master_clear(&master->fsm);
    ec_datagram_clear(&master->fsm_datagram);
    ec_device_clear(&master->backup_device);
    ec_device_clear(&master->main_device);

    // destroy self
    kobject_del(&master->kobj);
    kobject_put(&master->kobj);
}

/*****************************************************************************/

/**
 * Clear and free all EoE handlers.
 */

void ec_master_clear_eoe_handlers(
        ec_master_t *master /**< EtherCAT master */
        )
{
    ec_eoe_t *eoe, *next;

    list_for_each_entry_safe(eoe, next, &master->eoe_handlers, list) {
        list_del(&eoe->list);
        ec_eoe_clear(eoe);
        kfree(eoe);
    }
}

/*****************************************************************************/

/**
   Destroy all slaves.
*/

void ec_master_destroy_slaves(ec_master_t *master)
{
    ec_slave_t *slave, *next_slave;

    list_for_each_entry_safe(slave, next_slave, &master->slaves, list) {
        list_del(&slave->list);
        ec_slave_destroy(slave);
    }

    master->slave_count = 0;
}

/*****************************************************************************/

/**
   Destroy all domains.
*/

void ec_master_destroy_domains(ec_master_t *master)
{
    ec_domain_t *domain, *next_d;

    list_for_each_entry_safe(domain, next_d, &master->domains, list) {
        list_del(&domain->list);
        ec_domain_destroy(domain);
    }
}

/*****************************************************************************/

/**
   Internal locking callback.
*/

int ec_master_request_cb(void *master /**< callback data */)
{
    spin_lock(&((ec_master_t *) master)->internal_lock);
    return 0;
}

/*****************************************************************************/

/**
   Internal unlocking callback.
*/

void ec_master_release_cb(void *master /**< callback data */)
{
    spin_unlock(&((ec_master_t *) master)->internal_lock);
}

/*****************************************************************************/

/**
 * Starts the master thread.
 */

int ec_master_thread_start(
        ec_master_t *master, /**< EtherCAT master */
        int (*thread_func)(ec_master_t *) /**< thread function to start */
        )
{
    init_completion(&master->thread_exit);

    EC_INFO("Starting master thread.\n");
    if (!(master->thread_id = kernel_thread((int (*)(void *)) thread_func,
                    master, CLONE_KERNEL)))
        return -1;
    
    return 0;
}

/*****************************************************************************/

/**
 * Stops the master thread.
 */

void ec_master_thread_stop(ec_master_t *master /**< EtherCAT master */)
{
    if (!master->thread_id) {
        EC_WARN("ec_master_thread_stop: Already finished!\n");
        return;
    }

    kill_proc(master->thread_id, SIGTERM, 1);
    wait_for_completion(&master->thread_exit);
    EC_INFO("Master thread exited.\n");

    if (master->fsm_datagram.state != EC_DATAGRAM_SENT) return;
    
    // wait for FSM datagram
    while (get_cycles() - master->fsm_datagram.cycles_sent
            < (cycles_t) EC_IO_TIMEOUT /* us */ * (cpu_khz / 1000))
        schedule();
}

/*****************************************************************************/

/**
 * Transition function from ORPHANED to IDLE mode.
 */

int ec_master_enter_idle_mode(ec_master_t *master /**< EtherCAT master */)
{
    master->request_cb = ec_master_request_cb;
    master->release_cb = ec_master_release_cb;
    master->cb_data = master;

    master->mode = EC_MASTER_MODE_IDLE;
    if (ec_master_thread_start(master, ec_master_idle_thread)) {
        master->mode = EC_MASTER_MODE_ORPHANED;
        return -1;
    }

    return 0;
}

/*****************************************************************************/

/**
 * Transition function from IDLE to ORPHANED mode.
 */

void ec_master_leave_idle_mode(ec_master_t *master /**< EtherCAT master */)
{
    master->mode = EC_MASTER_MODE_ORPHANED;
    
    ec_master_eoe_stop(master);
    ec_master_thread_stop(master);
    ec_master_destroy_slaves(master);
}

/*****************************************************************************/

/**
 * Transition function from IDLE to OPERATION mode.
 */

int ec_master_enter_operation_mode(ec_master_t *master /**< EtherCAT master */)
{
    ec_slave_t *slave;
    ec_eoe_t *eoe;

    down(&master->config_sem);
    master->allow_config = 0; // temporarily disable slave configuration
    up(&master->config_sem);

    // wait for slave configuration to complete
    if (wait_event_interruptible(master->config_queue,
                master->config_state != EC_REQUEST_IN_PROGRESS)) {
        EC_INFO("Finishing slave configuration interrupted by signal.\n");
        goto out_allow;
    }

    if (master->debug_level)
        EC_DBG("Waiting for pending slave configuration returned.\n");

    down(&master->scan_sem);
    master->allow_scan = 0; // 'lock' the slave list
    up(&master->scan_sem);

    // wait for slave scan to complete
    if (wait_event_interruptible(master->scan_queue,
                master->scan_state != EC_REQUEST_IN_PROGRESS)) {
        EC_INFO("Waiting for slave scan interrupted by signal.\n");
        goto out_allow;
    }

    if (master->debug_level)
        EC_DBG("Waiting for pending slave scan returned.\n");

    // set states for all slaves
    list_for_each_entry(slave, &master->slaves, list) {
        ec_slave_request_state(slave, EC_SLAVE_STATE_PREOP);
    }
    // ... but set EoE slaves to OP
    list_for_each_entry(eoe, &master->eoe_handlers, list) {
        if (ec_eoe_is_open(eoe))
            ec_slave_request_state(eoe->slave, EC_SLAVE_STATE_OP);
    }

    if (master->debug_level)
        EC_DBG("Switching to operation mode.\n");

    master->mode = EC_MASTER_MODE_OPERATION;
    master->pdo_slaves_offline = 0; // assume all PDO slaves online
    master->frames_timed_out = 0;
    master->ext_request_cb = NULL;
    master->ext_release_cb = NULL;
    master->ext_cb_data = NULL;
    return 0;
    
out_allow:
    master->allow_scan = 1;
    master->allow_config = 1;
    return -1;
}

/*****************************************************************************/

/**
 * Transition function from OPERATION to IDLE mode.
 */

void ec_master_leave_operation_mode(ec_master_t *master
                                    /**< EtherCAT master */)
{
    ec_slave_t *slave;
    ec_eoe_t *eoe;

    master->mode = EC_MASTER_MODE_IDLE;

    ec_master_eoe_stop(master);
    ec_master_thread_stop(master);
    
    master->request_cb = ec_master_request_cb;
    master->release_cb = ec_master_release_cb;
    master->cb_data = master;
    
    // set states for all slaves
    list_for_each_entry(slave, &master->slaves, list) {
        ec_slave_reset(slave);
        ec_slave_request_state(slave, EC_SLAVE_STATE_PREOP);
    }
    // ... but leave EoE slaves in OP
    list_for_each_entry(eoe, &master->eoe_handlers, list) {
        if (ec_eoe_is_open(eoe))
            ec_slave_request_state(eoe->slave, EC_SLAVE_STATE_OP);
    }

    ec_master_destroy_domains(master);
    
    if (ec_master_thread_start(master, ec_master_idle_thread))
        EC_WARN("Failed to restart master thread!\n");
    ec_master_eoe_start(master);

    master->allow_scan = 1;
    master->allow_config = 1;
}

/*****************************************************************************/

/**
   Places a datagram in the datagram queue.
*/

void ec_master_queue_datagram(ec_master_t *master, /**< EtherCAT master */
                              ec_datagram_t *datagram /**< datagram */
                              )
{
    ec_datagram_t *queued_datagram;

    // check, if the datagram is already queued
    list_for_each_entry(queued_datagram, &master->datagram_queue, queue) {
        if (queued_datagram == datagram) {
            master->stats.skipped++;
            if (master->debug_level)
                EC_DBG("skipping datagram %x.\n", (unsigned int) datagram);
            ec_master_output_stats(master);
            datagram->state = EC_DATAGRAM_QUEUED;
            return;
        }
    }

    list_add_tail(&datagram->queue, &master->datagram_queue);
    datagram->state = EC_DATAGRAM_QUEUED;
}

/*****************************************************************************/

/**
   Sends the datagrams in the queue.
   \return 0 in case of success, else < 0
*/

void ec_master_send_datagrams(ec_master_t *master /**< EtherCAT master */)
{
    ec_datagram_t *datagram, *next;
    size_t datagram_size;
    uint8_t *frame_data, *cur_data;
    void *follows_word;
    cycles_t cycles_start, cycles_sent, cycles_end;
    unsigned long jiffies_sent;
    unsigned int frame_count, more_datagrams_waiting;
    struct list_head sent_datagrams;

    cycles_start = get_cycles();
    frame_count = 0;
    INIT_LIST_HEAD(&sent_datagrams);

    if (unlikely(master->debug_level > 1))
        EC_DBG("ec_master_send_datagrams\n");

    do {
        // fetch pointer to transmit socket buffer
        frame_data = ec_device_tx_data(&master->main_device);
        cur_data = frame_data + EC_FRAME_HEADER_SIZE;
        follows_word = NULL;
        more_datagrams_waiting = 0;

        // fill current frame with datagrams
        list_for_each_entry(datagram, &master->datagram_queue, queue) {
            if (datagram->state != EC_DATAGRAM_QUEUED) continue;

            // does the current datagram fit in the frame?
            datagram_size = EC_DATAGRAM_HEADER_SIZE + datagram->data_size
                + EC_DATAGRAM_FOOTER_SIZE;
            if (cur_data - frame_data + datagram_size > ETH_DATA_LEN) {
                more_datagrams_waiting = 1;
                break;
            }

            list_add_tail(&datagram->sent, &sent_datagrams);
            datagram->index = master->datagram_index++;

            if (unlikely(master->debug_level > 1))
                EC_DBG("adding datagram 0x%02X\n", datagram->index);

            // set "datagram following" flag in previous frame
            if (follows_word)
                EC_WRITE_U16(follows_word, EC_READ_U16(follows_word) | 0x8000);

            // EtherCAT datagram header
            EC_WRITE_U8 (cur_data,     datagram->type);
            EC_WRITE_U8 (cur_data + 1, datagram->index);
            EC_WRITE_U32(cur_data + 2, datagram->address.logical);
            EC_WRITE_U16(cur_data + 6, datagram->data_size & 0x7FF);
            EC_WRITE_U16(cur_data + 8, 0x0000);
            follows_word = cur_data + 6;
            cur_data += EC_DATAGRAM_HEADER_SIZE;

            // EtherCAT datagram data
            memcpy(cur_data, datagram->data, datagram->data_size);
            cur_data += datagram->data_size;

            // EtherCAT datagram footer
            EC_WRITE_U16(cur_data, 0x0000); // reset working counter
            cur_data += EC_DATAGRAM_FOOTER_SIZE;
        }

        if (list_empty(&sent_datagrams)) {
            if (unlikely(master->debug_level > 1))
                EC_DBG("nothing to send.\n");
            break;
        }

        // EtherCAT frame header
        EC_WRITE_U16(frame_data, ((cur_data - frame_data
                                   - EC_FRAME_HEADER_SIZE) & 0x7FF) | 0x1000);

        // pad frame
        while (cur_data - frame_data < ETH_ZLEN - ETH_HLEN)
            EC_WRITE_U8(cur_data++, 0x00);

        if (unlikely(master->debug_level > 1))
            EC_DBG("frame size: %i\n", cur_data - frame_data);

        // send frame
        ec_device_send(&master->main_device, cur_data - frame_data);
        cycles_sent = get_cycles();
        jiffies_sent = jiffies;

        // set datagram states and sending timestamps
        list_for_each_entry_safe(datagram, next, &sent_datagrams, sent) {
            datagram->state = EC_DATAGRAM_SENT;
            datagram->cycles_sent = cycles_sent;
            datagram->jiffies_sent = jiffies_sent;
            list_del_init(&datagram->sent); // empty list of sent datagrams
        }

        frame_count++;
    }
    while (more_datagrams_waiting);

    if (unlikely(master->debug_level > 1)) {
        cycles_end = get_cycles();
        EC_DBG("ec_master_send_datagrams sent %i frames in %ius.\n",
               frame_count,
               (unsigned int) (cycles_end - cycles_start) * 1000 / cpu_khz);
    }
}

/*****************************************************************************/

/**
   Processes a received frame.
   This function is called by the network driver for every received frame.
   \return 0 in case of success, else < 0
*/

void ec_master_receive_datagrams(ec_master_t *master, /**< EtherCAT master */
                                 const uint8_t *frame_data, /**< frame data */
                                 size_t size /**< size of the received data */
                                 )
{
    size_t frame_size, data_size;
    uint8_t datagram_type, datagram_index;
    unsigned int cmd_follows, matched;
    const uint8_t *cur_data;
    ec_datagram_t *datagram;

    if (unlikely(size < EC_FRAME_HEADER_SIZE)) {
        master->stats.corrupted++;
        ec_master_output_stats(master);
        return;
    }

    cur_data = frame_data;

    // check length of entire frame
    frame_size = EC_READ_U16(cur_data) & 0x07FF;
    cur_data += EC_FRAME_HEADER_SIZE;

    if (unlikely(frame_size > size)) {
        master->stats.corrupted++;
        ec_master_output_stats(master);
        return;
    }

    cmd_follows = 1;
    while (cmd_follows) {
        // process datagram header
        datagram_type  = EC_READ_U8 (cur_data);
        datagram_index = EC_READ_U8 (cur_data + 1);
        data_size      = EC_READ_U16(cur_data + 6) & 0x07FF;
        cmd_follows    = EC_READ_U16(cur_data + 6) & 0x8000;
        cur_data += EC_DATAGRAM_HEADER_SIZE;

        if (unlikely(cur_data - frame_data
                     + data_size + EC_DATAGRAM_FOOTER_SIZE > size)) {
            master->stats.corrupted++;
            ec_master_output_stats(master);
            return;
        }

        // search for matching datagram in the queue
        matched = 0;
        list_for_each_entry(datagram, &master->datagram_queue, queue) {
            if (datagram->index == datagram_index
                && datagram->state == EC_DATAGRAM_SENT
                && datagram->type == datagram_type
                && datagram->data_size == data_size) {
                matched = 1;
                break;
            }
        }

        // no matching datagram was found
        if (!matched) {
            master->stats.unmatched++;
            ec_master_output_stats(master);

            if (unlikely(master->debug_level > 0)) {
                EC_DBG("UNMATCHED datagram:\n");
                ec_print_data(cur_data - EC_DATAGRAM_HEADER_SIZE,
                        EC_DATAGRAM_HEADER_SIZE + data_size
                        + EC_DATAGRAM_FOOTER_SIZE);
            }

            cur_data += data_size + EC_DATAGRAM_FOOTER_SIZE;
            continue;
        }

        // copy received data into the datagram memory
        memcpy(datagram->data, cur_data, data_size);
        cur_data += data_size;

        // set the datagram's working counter
        datagram->working_counter = EC_READ_U16(cur_data);
        cur_data += EC_DATAGRAM_FOOTER_SIZE;

        // dequeue the received datagram
        datagram->state = EC_DATAGRAM_RECEIVED;
        datagram->cycles_received = master->main_device.cycles_poll;
        datagram->jiffies_received = master->main_device.jiffies_poll;
        list_del_init(&datagram->queue);
    }
}

/*****************************************************************************/

/**
   Output statistics in cyclic mode.
   This function outputs statistical data on demand, but not more often than
   necessary. The output happens at most once a second.
*/

void ec_master_output_stats(ec_master_t *master /**< EtherCAT master */)
{
    if (unlikely(jiffies - master->stats.output_jiffies >= HZ)) {
        master->stats.output_jiffies = jiffies;

        if (master->stats.timeouts) {
            EC_WARN("%i datagram%s TIMED OUT!\n", master->stats.timeouts,
                    master->stats.timeouts == 1 ? "" : "s");
            master->stats.timeouts = 0;
        }
        if (master->stats.corrupted) {
            EC_WARN("%i frame%s CORRUPTED!\n", master->stats.corrupted,
                    master->stats.corrupted == 1 ? "" : "s");
            master->stats.corrupted = 0;
        }
        if (master->stats.skipped) {
            EC_WARN("%i datagram%s SKIPPED!\n", master->stats.skipped,
                    master->stats.skipped == 1 ? "" : "s");
            master->stats.skipped = 0;
        }
        if (master->stats.unmatched) {
            EC_WARN("%i datagram%s UNMATCHED!\n", master->stats.unmatched,
                    master->stats.unmatched == 1 ? "" : "s");
            master->stats.unmatched = 0;
        }
    }
}

/*****************************************************************************/

/**
 * Master kernel thread function for IDLE mode.
 */

static int ec_master_idle_thread(ec_master_t *master)
{
    cycles_t cycles_start, cycles_end;

    daemonize("EtherCAT-IDLE");
    allow_signal(SIGTERM);

    while (!signal_pending(current)) {
        cycles_start = get_cycles();

        if (ec_fsm_master_running(&master->fsm)) {
            // receive
            spin_lock_bh(&master->internal_lock);
            ecrt_master_receive(master);
            spin_unlock_bh(&master->internal_lock);
        }

        if (master->fsm_datagram.state == EC_DATAGRAM_SENT)
            goto schedule;

        // execute master state machine
        if (ec_fsm_master_exec(&master->fsm)) {
            // queue and send
            ec_master_queue_datagram(master, &master->fsm_datagram);
            spin_lock_bh(&master->internal_lock);
            ecrt_master_send(master);
            spin_unlock_bh(&master->internal_lock);
        }
        
        cycles_end = get_cycles();
        master->idle_cycle_times[master->idle_cycle_time_pos]
            = (u32) (cycles_end - cycles_start) * 1000 / cpu_khz;
        master->idle_cycle_time_pos++;
        master->idle_cycle_time_pos %= HZ;

schedule:
        if (ec_fsm_master_idle(&master->fsm)) {
            set_current_state(TASK_INTERRUPTIBLE);
            schedule_timeout(1);
        }
        else {
            schedule();
        }
    }
    
    master->thread_id = 0;
    if (master->debug_level)
        EC_DBG("Master IDLE thread exiting...\n");
    complete_and_exit(&master->thread_exit, 0);
}

/*****************************************************************************/

/**
 * Master kernel thread function for IDLE mode.
 */

static int ec_master_operation_thread(ec_master_t *master)
{
    cycles_t cycles_start, cycles_end;

    daemonize("EtherCAT-OP");
    allow_signal(SIGTERM);

    while (!signal_pending(current)) {
        if (master->injection_seq_rt != master->injection_seq_fsm ||
                master->fsm_datagram.state == EC_DATAGRAM_SENT ||
                master->fsm_datagram.state == EC_DATAGRAM_QUEUED)
            goto schedule;

        cycles_start = get_cycles();

        // output statistics
        ec_master_output_stats(master);

        // execute master state machine
        if (ec_fsm_master_exec(&master->fsm)) {
            // inject datagram
            master->injection_seq_fsm++;
        }

        cycles_end = get_cycles();
        master->idle_cycle_times[master->idle_cycle_time_pos]
            = (u32) (cycles_end - cycles_start) * 1000 / cpu_khz;
        master->idle_cycle_time_pos++;
        master->idle_cycle_time_pos %= HZ;

schedule:
        if (ec_fsm_master_idle(&master->fsm)) {
            set_current_state(TASK_INTERRUPTIBLE);
            schedule_timeout(1);
        }
        else {
            schedule();
        }
    }
    
    master->thread_id = 0;
    if (master->debug_level)
        EC_DBG("Master OP thread exiting...\n");
    complete_and_exit(&master->thread_exit, 0);
}

/*****************************************************************************/

ssize_t ec_master_device_info(
        const ec_device_t *device,
        const uint8_t *mac,
        char *buffer
        )
{
    unsigned int frames_lost;
    off_t off = 0;
    
    if (ec_mac_is_zero(mac)) {
        off += sprintf(buffer + off, "none.\n");
    }
    else {
        off += ec_mac_print(mac, buffer + off);
    
        if (device->dev) {
            off += sprintf(buffer + off, " (connected).\n");      
            off += sprintf(buffer + off, "    Frames sent:     %u\n",
                    device->tx_count);
            off += sprintf(buffer + off, "    Frames received: %u\n",
                    device->rx_count);
            frames_lost = device->tx_count - device->rx_count;
            if (frames_lost) frames_lost--;
            off += sprintf(buffer + off, "    Frames lost:     %u\n", frames_lost);
        }
        else {
            off += sprintf(buffer + off, " (WAITING).\n");      
        }
    }
    
    return off;
}

/*****************************************************************************/

/**
   Formats master information for SysFS read access.
   \return number of bytes written
*/

ssize_t ec_master_info(ec_master_t *master, /**< EtherCAT master */
                       char *buffer /**< memory to store data */
                       )
{
    off_t off = 0;
    ec_eoe_t *eoe;
    uint32_t cur, sum, min, max, pos, i;

    off += sprintf(buffer + off, "\nMode: ");
    switch (master->mode) {
        case EC_MASTER_MODE_ORPHANED:
            off += sprintf(buffer + off, "ORPHANED");
            break;
        case EC_MASTER_MODE_IDLE:
            off += sprintf(buffer + off, "IDLE");
            break;
        case EC_MASTER_MODE_OPERATION:
            off += sprintf(buffer + off, "OPERATION");
            break;
    }

    off += sprintf(buffer + off, "\nSlaves: %i\n",
                   master->slave_count);

    off += sprintf(buffer + off, "\nDevices:\n");
    
    down(&master->device_sem);
    off += sprintf(buffer + off, "  Main: ");
    off += ec_master_device_info(&master->main_device,
            master->main_mac, buffer + off);
    off += sprintf(buffer + off, "  Backup: ");
    off += ec_master_device_info(&master->backup_device,
            master->backup_mac, buffer + off);
    up(&master->device_sem);

    off += sprintf(buffer + off, "\nTiming (min/avg/max) [us]:\n");

    sum = 0;
    min = 0xFFFFFFFF;
    max = 0;
    pos = master->idle_cycle_time_pos;
    for (i = 0; i < HZ; i++) {
        cur = master->idle_cycle_times[(i + pos) % HZ];
        sum += cur;
        if (cur < min) min = cur;
        if (cur > max) max = cur;
    }
    off += sprintf(buffer + off, "  Idle cycle: %u / %u.%u / %u\n",
                   min, sum / HZ, (sum * 100 / HZ) % 100, max);

    sum = 0;
    min = 0xFFFFFFFF;
    max = 0;
    pos = master->eoe_cycle_time_pos;
    for (i = 0; i < HZ; i++) {
        cur = master->eoe_cycle_times[(i + pos) % HZ];
        sum += cur;
        if (cur < min) min = cur;
        if (cur > max) max = cur;
    }
    off += sprintf(buffer + off, "  EoE cycle: %u / %u.%u / %u\n",
                   min, sum / HZ, (sum * 100 / HZ) % 100, max);

    if (!list_empty(&master->eoe_handlers))
        off += sprintf(buffer + off, "\nEoE statistics (RX/TX) [bps]:\n");
    list_for_each_entry(eoe, &master->eoe_handlers, list) {
        off += sprintf(buffer + off, "  %s: %u / %u (%u KB/s)\n",
                       eoe->dev->name, eoe->rx_rate, eoe->tx_rate,
                       ((eoe->rx_rate + eoe->tx_rate) / 8 + 512) / 1024);
    }

    off += sprintf(buffer + off, "\n");

    return off;
}

/*****************************************************************************/

/**
   Formats attribute data for SysFS read access.
   \return number of bytes to read
*/

ssize_t ec_show_master_attribute(struct kobject *kobj, /**< kobject */
                                 struct attribute *attr, /**< attribute */
                                 char *buffer /**< memory to store data */
                                 )
{
    ec_master_t *master = container_of(kobj, ec_master_t, kobj);

    if (attr == &attr_info) {
        return ec_master_info(master, buffer);
    }
    else if (attr == &attr_debug_level) {
        return sprintf(buffer, "%i\n", master->debug_level);
    }

    return 0;
}

/*****************************************************************************/

/**
   Formats attribute data for SysFS write access.
   \return number of bytes processed, or negative error code
*/

ssize_t ec_store_master_attribute(struct kobject *kobj, /**< slave's kobject */
                                  struct attribute *attr, /**< attribute */
                                  const char *buffer, /**< memory with data */
                                  size_t size /**< size of data to store */
                                  )
{
    ec_master_t *master = container_of(kobj, ec_master_t, kobj);

    if (attr == &attr_debug_level) {
        if (!strcmp(buffer, "0\n")) {
            master->debug_level = 0;
        }
        else if (!strcmp(buffer, "1\n")) {
            master->debug_level = 1;
        }
        else if (!strcmp(buffer, "2\n")) {
            master->debug_level = 2;
        }
        else {
            EC_ERR("Invalid debug level value!\n");
            return -EINVAL;
        }

        EC_INFO("Master debug level set to %i.\n", master->debug_level);
        return size;
    }

    return -EINVAL;
}

/*****************************************************************************/

/**
   Starts Ethernet-over-EtherCAT processing on demand.
*/

void ec_master_eoe_start(ec_master_t *master /**< EtherCAT master */)
{
    if (master->eoe_running) {
        EC_WARN("EoE already running!\n");
        return;
    }

    if (list_empty(&master->eoe_handlers))
        return;

    if (!master->request_cb || !master->release_cb) {
        EC_WARN("No EoE processing because of missing locking callbacks!\n");
        return;
    }

    EC_INFO("Starting EoE processing.\n");
    master->eoe_running = 1;

    // start EoE processing
    master->eoe_timer.expires = jiffies + 10;
    add_timer(&master->eoe_timer);
}

/*****************************************************************************/

/**
   Stops the Ethernet-over-EtherCAT processing.
*/

void ec_master_eoe_stop(ec_master_t *master /**< EtherCAT master */)
{
    if (!master->eoe_running) return;

    EC_INFO("Stopping EoE processing.\n");

    del_timer_sync(&master->eoe_timer);
    master->eoe_running = 0;
}

/*****************************************************************************/

/**
   Does the Ethernet-over-EtherCAT processing.
*/

void ec_master_eoe_run(unsigned long data /**< master pointer */)
{
    ec_master_t *master = (ec_master_t *) data;
    ec_eoe_t *eoe;
    unsigned int none_open = 1;
    cycles_t cycles_start, cycles_end;
    unsigned long restart_jiffies;

    list_for_each_entry(eoe, &master->eoe_handlers, list) {
        if (ec_eoe_is_open(eoe)) {
            none_open = 0;
            break;
        }
    }
    if (none_open)
        goto queue_timer;

    if (master->request_cb(master->cb_data)) goto queue_timer;

    // receive datagrams
    cycles_start = get_cycles();
    ecrt_master_receive(master);

    // actual EoE processing
    list_for_each_entry(eoe, &master->eoe_handlers, list) {
        ec_eoe_run(eoe);
    }

    // send datagrams
    ecrt_master_send(master);
    cycles_end = get_cycles();

    master->release_cb(master->cb_data);

    master->eoe_cycle_times[master->eoe_cycle_time_pos]
        = (u32) (cycles_end - cycles_start) * 1000 / cpu_khz;
    master->eoe_cycle_time_pos++;
    master->eoe_cycle_time_pos %= HZ;

 queue_timer:
    restart_jiffies = HZ / EC_EOE_FREQUENCY;
    if (!restart_jiffies) restart_jiffies = 1;
    master->eoe_timer.expires += restart_jiffies;
    add_timer(&master->eoe_timer);
}

/*****************************************************************************/

/**
   Measures the time, a frame is on the bus.
   \return 0 in case of success, else < 0
*/

int ec_master_measure_bus_time(ec_master_t *master)
{
    ec_datagram_t datagram;
    uint32_t cur, sum, min, max, i;

    ec_datagram_init(&datagram);

    if (ec_datagram_brd(&datagram, 0x0130, 2)) {
        EC_ERR("Failed to allocate datagram for bus time measuring.\n");
        ec_datagram_clear(&datagram);
        return -1;
    }

    ecrt_master_receive(master);

    sum = 0;
    min = 0xFFFFFFFF;
    max = 0;

    for (i = 0; i < 100; i++) {
        ec_master_queue_datagram(master, &datagram);
        ecrt_master_send(master);

        while (1) {
            ecrt_master_receive(master);

            if (datagram.state == EC_DATAGRAM_RECEIVED) {
                break;
            }
            else if (datagram.state == EC_DATAGRAM_ERROR) {
                EC_WARN("Failed to measure bus time.\n");
                goto error;
            }
            else if (datagram.state == EC_DATAGRAM_TIMED_OUT) {
                EC_WARN("Timeout while measuring bus time.\n");
                goto error;
            }
        }

        cur = (unsigned int) (datagram.cycles_received
                              - datagram.cycles_sent) * 1000 / cpu_khz;
        sum += cur;
        if (cur > max) max = cur;
        if (cur < min) min = cur;
    }

    EC_DBG("Bus time is (min/avg/max) %u / %u.%u / %u us.\n",
           min, sum / 100, sum % 100, max);
    ec_datagram_clear(&datagram);
    return 0;

  error:
    ec_datagram_clear(&datagram);
    return -1;
}

/*****************************************************************************/

/**
   Prepares synchronous IO.
   Queues all domain datagrams and sends them. Then waits a certain time, so
   that ecrt_master_receive() can be called securely.
*/

void ec_master_prepare(ec_master_t *master /**< EtherCAT master */)
{
    ec_domain_t *domain;
    cycles_t cycles_start, cycles_end, cycles_timeout;

    // queue datagrams of all domains
    list_for_each_entry(domain, &master->domains, list)
        ecrt_domain_queue(domain);

    ecrt_master_send(master);

    cycles_start = get_cycles();
    cycles_timeout = (cycles_t) EC_IO_TIMEOUT /* us */ * (cpu_khz / 1000);

    // active waiting
    while (1) {
        udelay(100);
        cycles_end = get_cycles();
        if (cycles_end - cycles_start >= cycles_timeout) break;
    }
}

/*****************************************************************************/

/**
 * Translates an ASCII coded bus-address to a slave pointer.
 * These are the valid addressing schemes:
 * - \a "X" = the Xth slave on the bus (ring position),
 * - \a "#X" = the slave with alias X,
 * - \a "#X:Y" = the Yth slave after the slave with alias X.
 * X and Y are zero-based indices and may be provided in hexadecimal or octal
 * notation (with appropriate prefix).
 * \return pointer to the slave on success, else NULL
 */

ec_slave_t *ec_master_parse_slave_address(
        const ec_master_t *master, /**< EtherCAT master */
        const char *address /**< address string */
        )
{
    unsigned long first, second;
    char *remainder, *remainder2;
    const char *original;
    unsigned int alias_requested = 0, alias_not_found = 1;
    ec_slave_t *alias_slave = NULL, *slave;

    original = address;

    if (!address[0])
        goto out_invalid;

    if (address[0] == '#') {
        alias_requested = 1;
        address++;
    }

    first = simple_strtoul(address, &remainder, 0);
    if (remainder == address)
        goto out_invalid;

    if (alias_requested) {
        list_for_each_entry(alias_slave, &master->slaves, list) {
            if (alias_slave->sii_alias == first) {
                alias_not_found = 0;
                break;
            }
        }
        if (alias_not_found) {
            EC_ERR("Alias not found!\n");
            goto out_invalid;
        }
    }

    if (!remainder[0]) {
        if (alias_requested) { // alias addressing
            return alias_slave;
        }
        else { // position addressing
            list_for_each_entry(slave, &master->slaves, list) {
                if (slave->ring_position == first) return slave;
            }
            EC_ERR("Slave index out of range!\n");
            goto out_invalid;
        }
    }
    else if (alias_requested && remainder[0] == ':') { // field addressing
        struct list_head *list;
        remainder++;
        second = simple_strtoul(remainder, &remainder2, 0);

        if (remainder2 == remainder || remainder2[0])
            goto out_invalid;

        list = &alias_slave->list;
        while (second--) {
            list = list->next;
            if (list == &master->slaves) { // last slave exceeded
                EC_ERR("Slave index out of range!\n");
                goto out_invalid;
            }
        }
        return list_entry(list, ec_slave_t, list);
    }

out_invalid:
    EC_ERR("Invalid slave address string \"%s\"!\n", original);
    return NULL;
}

/******************************************************************************
 *  Realtime interface
 *****************************************************************************/

/**
   Creates a domain.
   \return pointer to new domain on success, else NULL
   \ingroup RealtimeInterface
*/

ec_domain_t *ecrt_master_create_domain(ec_master_t *master /**< master */)
{
    ec_domain_t *domain, *last_domain;
    unsigned int index;

    if (!(domain = (ec_domain_t *) kmalloc(sizeof(ec_domain_t), GFP_KERNEL))) {
        EC_ERR("Error allocating domain memory!\n");
        return NULL;
    }

    if (list_empty(&master->domains)) index = 0;
    else {
        last_domain = list_entry(master->domains.prev, ec_domain_t, list);
        index = last_domain->index + 1;
    }

    if (ec_domain_init(domain, master, index)) {
        EC_ERR("Failed to init domain.\n");
        return NULL;
    }

    list_add_tail(&domain->list, &master->domains);

    return domain;
}

/*****************************************************************************/

/**
   Configures all slaves and leads them to the OP state.
   Does the complete configuration and activation for all slaves. Sets sync
   managers and FMMUs, and does the appropriate transitions, until the slave
   is operational.
   \return 0 in case of success, else < 0
   \ingroup RealtimeInterface
*/

int ecrt_master_activate(ec_master_t *master /**< EtherCAT master */)
{
    uint32_t domain_offset;
    ec_domain_t *domain;

    // allocate all domains
    domain_offset = 0;
    list_for_each_entry(domain, &master->domains, list) {
        if (ec_domain_alloc(domain, domain_offset)) {
            EC_ERR("Failed to allocate domain %X!\n", (u32) domain);
            return -1;
        }
        domain_offset += domain->data_size;
    }

    // request slave configuration
    down(&master->config_sem);
    master->allow_config = 1; // request the current configuration
    master->config_state = EC_REQUEST_IN_PROGRESS;
    up(&master->config_sem);

    // wait for configuration to complete
    if (wait_event_interruptible(master->config_queue,
                master->config_state != EC_REQUEST_IN_PROGRESS)) {
        EC_INFO("Waiting for configuration interrupted by signal.\n");
        return -1;
    }

    if (master->config_state != EC_REQUEST_COMPLETE) {
        EC_ERR("Failed to configure slaves.\n");
        return -1;
    }
    
    // restart EoE process and master thread with new locking
    ec_master_eoe_stop(master);
    ec_master_thread_stop(master);

    ec_master_prepare(master); // prepare asynchronous IO

    if (master->debug_level)
        EC_DBG("FSM datagram is %x.\n", (unsigned int) &master->fsm_datagram);

    master->injection_seq_fsm = 0;
    master->injection_seq_rt = 0;
    master->request_cb = master->ext_request_cb;
    master->release_cb = master->ext_release_cb;
    master->cb_data = master->ext_cb_data;
    
    if (ec_master_thread_start(master, ec_master_operation_thread)) {
        EC_ERR("Failed to start master thread!\n");
        return -1;
    }
    ec_master_eoe_start(master);
    return 0;
}

/*****************************************************************************/

/**
   Asynchronous sending of datagrams.
   \ingroup RealtimeInterface
*/

void ecrt_master_send(ec_master_t *master /**< EtherCAT master */)
{
    ec_datagram_t *datagram, *n;

    if (master->injection_seq_rt != master->injection_seq_fsm) {
        // inject datagram produced by master FSM
        ec_master_queue_datagram(master, &master->fsm_datagram);
        master->injection_seq_rt = master->injection_seq_fsm;
    }

    if (unlikely(!master->main_device.link_state)) {
        // link is down, no datagram can be sent
        list_for_each_entry_safe(datagram, n, &master->datagram_queue, queue) {
            datagram->state = EC_DATAGRAM_ERROR;
            list_del_init(&datagram->queue);
        }

        // query link state
        ec_device_poll(&master->main_device);
        return;
    }

    // send frames
    ec_master_send_datagrams(master);
}

/*****************************************************************************/

/**
   Asynchronous receiving of datagrams.
   \ingroup RealtimeInterface
*/

void ecrt_master_receive(ec_master_t *master /**< EtherCAT master */)
{
    ec_datagram_t *datagram, *next;
    cycles_t cycles_timeout;
    unsigned int frames_timed_out = 0;

    // receive datagrams
    ec_device_poll(&master->main_device);

    cycles_timeout = (cycles_t) EC_IO_TIMEOUT /* us */ * (cpu_khz / 1000);

    // dequeue all datagrams that timed out
    list_for_each_entry_safe(datagram, next, &master->datagram_queue, queue) {
        if (datagram->state != EC_DATAGRAM_SENT) continue;

        if (master->main_device.cycles_poll - datagram->cycles_sent
            > cycles_timeout) {
            frames_timed_out = 1;
            list_del_init(&datagram->queue);
            datagram->state = EC_DATAGRAM_TIMED_OUT;
            master->stats.timeouts++;
            ec_master_output_stats(master);

            if (unlikely(master->debug_level > 0)) {
                EC_DBG("TIMED OUT datagram index %02X waited %u us.\n",
                        datagram->index,
                        (unsigned int) (master->main_device.cycles_poll
                            - datagram->cycles_sent) * 1000 / cpu_khz);
            }
        }
    }

    master->frames_timed_out = frames_timed_out;
}

/*****************************************************************************/

/**
 * Obtains a slave pointer by its bus address.
 * A valid slave pointer is only returned, if vendor ID and product code are
 * matching.
 * \return pointer to the slave on success, else NULL
 * \ingroup RealtimeInterface
 */

ec_slave_t *ecrt_master_get_slave(
        const ec_master_t *master, /**< EtherCAT master */
        const char *address, /**< address string
                               \see ec_master_parse_slave_address() */
        uint32_t vendor_id, /**< vendor ID */
        uint32_t product_code /**< product code */
        )
{
    ec_slave_t *slave = ec_master_parse_slave_address(master, address);

    if (!slave)
        return NULL;

    return ec_slave_validate(slave, vendor_id, product_code) ? NULL : slave;
}

/*****************************************************************************/

/**
   Sets the locking callbacks.
   The request_cb function must return zero, to allow another instance
   (the EoE process for example) to access the master. Non-zero means,
   that access is forbidden at this time.
   \ingroup RealtimeInterface
*/

void ecrt_master_callbacks(ec_master_t *master, /**< EtherCAT master */
                           int (*request_cb)(void *), /**< request lock CB */
                           void (*release_cb)(void *), /**< release lock CB */
                           void *cb_data /**< data parameter */
                           )
{
    master->ext_request_cb = request_cb;
    master->ext_release_cb = release_cb;
    master->ext_cb_data = cb_data;
}

/*****************************************************************************/

/**
 * Reads the current master status.
 */

void ecrt_master_get_status(const ec_master_t *master, /**< EtherCAT master */
        ec_master_status_t *status /**< target status object */
        )
{
    status->bus_status =
        (master->pdo_slaves_offline || master->frames_timed_out)
        ? EC_BUS_FAILURE : EC_BUS_OK;
    status->bus_tainted = master->fsm.tainted; 
    status->slaves_responding = master->fsm.slaves_responding;
}

/*****************************************************************************/

/** \cond */

EXPORT_SYMBOL(ecrt_master_create_domain);
EXPORT_SYMBOL(ecrt_master_activate);
EXPORT_SYMBOL(ecrt_master_send);
EXPORT_SYMBOL(ecrt_master_receive);
EXPORT_SYMBOL(ecrt_master_callbacks);
EXPORT_SYMBOL(ecrt_master_get_slave);
EXPORT_SYMBOL(ecrt_master_get_status);

/** \endcond */

/*****************************************************************************/
