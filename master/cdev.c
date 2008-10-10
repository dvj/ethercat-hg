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
   EtherCAT master character device.
*/

/*****************************************************************************/

#include <linux/module.h>
#include <linux/vmalloc.h>
#include <linux/mm.h>

#include "cdev.h"
#include "master.h"
#include "slave_config.h"
#include "ioctl.h"

/*****************************************************************************/

/** \cond */

int eccdev_open(struct inode *, struct file *);
int eccdev_release(struct inode *, struct file *);
long eccdev_ioctl(struct file *, unsigned int, unsigned long);
int eccdev_mmap(struct file *, struct vm_area_struct *);

static struct page *eccdev_vma_nopage(
        struct vm_area_struct *, unsigned long, int *);

/*****************************************************************************/

static struct file_operations eccdev_fops = {
    .owner          = THIS_MODULE,
    .open           = eccdev_open,
    .release        = eccdev_release,
    .unlocked_ioctl = eccdev_ioctl,
    .mmap           = eccdev_mmap
};

struct vm_operations_struct eccdev_vm_ops = {
    .nopage = eccdev_vma_nopage
};

/** \endcond */

/*****************************************************************************/

/** Private data structure for file handles.
 */
typedef struct {
    ec_cdev_t *cdev;
    unsigned int requested;
    uint8_t *process_data;
    size_t process_data_size;
} ec_cdev_priv_t;

/*****************************************************************************/

/** Constructor.
 * 
 * \return 0 in case of success, else < 0
 */
int ec_cdev_init(
		ec_cdev_t *cdev, /**< EtherCAT master character device. */
		ec_master_t *master, /**< Parent master. */
		dev_t dev_num /**< Device number. */
		)
{
    cdev->master = master;

    cdev_init(&cdev->cdev, &eccdev_fops);
    cdev->cdev.owner = THIS_MODULE;

    if (cdev_add(&cdev->cdev,
		 MKDEV(MAJOR(dev_num), master->index), 1)) {
		EC_ERR("Failed to add character device!\n");
		return -1;
    }

    return 0;
}

/*****************************************************************************/

/** Destructor.
 */
void ec_cdev_clear(ec_cdev_t *cdev /**< EtherCAT XML device */)
{
    cdev_del(&cdev->cdev);
}

/*****************************************************************************/

/** Copies a string to an ioctl structure.
 */
void ec_cdev_strcpy(
        char *target, /**< Target. */
        const char *source /**< Source. */
        )
{
    if (source) {
        strncpy(target, source, EC_IOCTL_STRING_SIZE);
        target[EC_IOCTL_STRING_SIZE - 1] = 0;
    } else {
        target[0] = 0;
    }
}

/*****************************************************************************/

/** Get master information.
 */
int ec_cdev_ioctl_master(
        ec_master_t *master, /**< EtherCAT master. */
        unsigned long arg /**< Userspace address to store the results. */
        )
{
    ec_ioctl_master_t data;

    if (down_interruptible(&master->master_sem))
        return -EINTR;
    data.slave_count = master->slave_count;
    data.config_count = ec_master_config_count(master);
    data.domain_count = ec_master_domain_count(master);
    data.phase = (uint8_t) master->phase;
    up(&master->master_sem);

    if (down_interruptible(&master->device_sem))
        return -EINTR;
    if (master->main_device.dev) {
        memcpy(data.devices[0].address,
                master->main_device.dev->dev_addr, ETH_ALEN);
    } else {
        memcpy(data.devices[0].address, master->main_mac, ETH_ALEN); 
    }
    data.devices[0].attached = master->main_device.dev ? 1 : 0;
    data.devices[0].tx_count = master->main_device.tx_count;
    data.devices[0].rx_count = master->main_device.rx_count;

    if (master->backup_device.dev) {
        memcpy(data.devices[1].address,
                master->backup_device.dev->dev_addr, ETH_ALEN); 
    } else {
        memcpy(data.devices[1].address, master->backup_mac, ETH_ALEN); 
    }
    data.devices[1].attached = master->backup_device.dev ? 1 : 0;
    data.devices[1].tx_count = master->backup_device.tx_count;
    data.devices[1].rx_count = master->backup_device.rx_count;
    up(&master->device_sem);

    if (copy_to_user((void __user *) arg, &data, sizeof(data)))
        return -EFAULT;

    return 0;
}

/*****************************************************************************/

/** Get slave information.
 */
int ec_cdev_ioctl_slave(
        ec_master_t *master, /**< EtherCAT master. */
        unsigned long arg /**< Userspace address to store the results. */
        )
{
    ec_ioctl_slave_t data;
    const ec_slave_t *slave;

    if (copy_from_user(&data, (void __user *) arg, sizeof(data))) {
        return -EFAULT;
    }

    if (down_interruptible(&master->master_sem))
        return -EINTR;

    if (!(slave = ec_master_find_slave_const(
                    master, 0, data.position))) {
        up(&master->master_sem);
        EC_ERR("Slave %u does not exist!\n", data.position);
        return -EINVAL;
    }

    data.vendor_id = slave->sii.vendor_id;
    data.product_code = slave->sii.product_code;
    data.revision_number = slave->sii.revision_number;
    data.serial_number = slave->sii.serial_number;
    data.alias = slave->sii.alias;
    data.rx_mailbox_offset = slave->sii.rx_mailbox_offset;
    data.rx_mailbox_size = slave->sii.rx_mailbox_size;
    data.tx_mailbox_offset = slave->sii.tx_mailbox_offset;
    data.tx_mailbox_size = slave->sii.tx_mailbox_size;
    data.mailbox_protocols = slave->sii.mailbox_protocols;
    data.has_general_category = slave->sii.has_general;
    data.coe_details = slave->sii.coe_details;
    data.general_flags = slave->sii.general_flags;
    data.current_on_ebus = slave->sii.current_on_ebus;
    data.al_state = slave->current_state;
    data.error_flag = slave->error_flag;

    data.sync_count = slave->sii.sync_count;
    data.sdo_count = ec_slave_sdo_count(slave);
    data.sii_nwords = slave->sii_nwords;
    ec_cdev_strcpy(data.group, slave->sii.group);
    ec_cdev_strcpy(data.image, slave->sii.image);
    ec_cdev_strcpy(data.order, slave->sii.order);
    ec_cdev_strcpy(data.name, slave->sii.name);

    up(&master->master_sem);

    if (copy_to_user((void __user *) arg, &data, sizeof(data)))
        return -EFAULT;

    return 0;
}

/*****************************************************************************/

/** Get slave sync manager information.
 */
int ec_cdev_ioctl_slave_sync(
        ec_master_t *master, /**< EtherCAT master. */
        unsigned long arg /**< Userspace address to store the results. */
        )
{
    ec_ioctl_slave_sync_t data;
    const ec_slave_t *slave;
    const ec_sync_t *sync;

    if (copy_from_user(&data, (void __user *) arg, sizeof(data))) {
        return -EFAULT;
    }

    if (down_interruptible(&master->master_sem))
        return -EINTR;

    if (!(slave = ec_master_find_slave_const(
                    master, 0, data.slave_position))) {
        up(&master->master_sem);
        EC_ERR("Slave %u does not exist!\n", data.slave_position);
        return -EINVAL;
    }

    if (data.sync_index >= slave->sii.sync_count) {
        up(&master->master_sem);
        EC_ERR("Sync manager %u does not exist in slave %u!\n",
                data.sync_index, data.slave_position);
        return -EINVAL;
    }

    sync = &slave->sii.syncs[data.sync_index];

    data.physical_start_address = sync->physical_start_address;
    data.default_size = sync->default_length;
    data.control_register = sync->control_register;
    data.enable = sync->enable;
    data.pdo_count = ec_pdo_list_count(&sync->pdos);

    up(&master->master_sem);

    if (copy_to_user((void __user *) arg, &data, sizeof(data)))
        return -EFAULT;

    return 0;
}

/*****************************************************************************/

/** Get slave sync manager Pdo information.
 */
int ec_cdev_ioctl_slave_sync_pdo(
        ec_master_t *master, /**< EtherCAT master. */
        unsigned long arg /**< Userspace address to store the results. */
        )
{
    ec_ioctl_slave_sync_pdo_t data;
    const ec_slave_t *slave;
    const ec_sync_t *sync;
    const ec_pdo_t *pdo;

    if (copy_from_user(&data, (void __user *) arg, sizeof(data))) {
        return -EFAULT;
    }

    if (down_interruptible(&master->master_sem))
        return -EINTR;

    if (!(slave = ec_master_find_slave_const(
                    master, 0, data.slave_position))) {
        up(&master->master_sem);
        EC_ERR("Slave %u does not exist!\n", data.slave_position);
        return -EINVAL;
    }

    if (data.sync_index >= slave->sii.sync_count) {
        up(&master->master_sem);
        EC_ERR("Sync manager %u does not exist in slave %u!\n",
                data.sync_index, data.slave_position);
        return -EINVAL;
    }

    sync = &slave->sii.syncs[data.sync_index];
    if (!(pdo = ec_pdo_list_find_pdo_by_pos_const(
                    &sync->pdos, data.pdo_pos))) {
        up(&master->master_sem);
        EC_ERR("Sync manager %u does not contain a Pdo with "
                "position %u in slave %u!\n", data.sync_index,
                data.pdo_pos, data.slave_position);
        return -EINVAL;
    }

    data.index = pdo->index;
    data.entry_count = ec_pdo_entry_count(pdo);
    ec_cdev_strcpy(data.name, pdo->name);

    up(&master->master_sem);

    if (copy_to_user((void __user *) arg, &data, sizeof(data)))
        return -EFAULT;

    return 0;
}

/*****************************************************************************/

/** Get slave sync manager Pdo entry information.
 */
int ec_cdev_ioctl_slave_sync_pdo_entry(
        ec_master_t *master, /**< EtherCAT master. */
        unsigned long arg /**< Userspace address to store the results. */
        )
{
    ec_ioctl_slave_sync_pdo_entry_t data;
    const ec_slave_t *slave;
    const ec_sync_t *sync;
    const ec_pdo_t *pdo;
    const ec_pdo_entry_t *entry;

    if (copy_from_user(&data, (void __user *) arg, sizeof(data))) {
        return -EFAULT;
    }

    if (down_interruptible(&master->master_sem))
        return -EINTR;

    if (!(slave = ec_master_find_slave_const(
                    master, 0, data.slave_position))) {
        up(&master->master_sem);
        EC_ERR("Slave %u does not exist!\n", data.slave_position);
        return -EINVAL;
    }

    if (data.sync_index >= slave->sii.sync_count) {
        up(&master->master_sem);
        EC_ERR("Sync manager %u does not exist in slave %u!\n",
                data.sync_index, data.slave_position);
        return -EINVAL;
    }

    sync = &slave->sii.syncs[data.sync_index];
    if (!(pdo = ec_pdo_list_find_pdo_by_pos_const(
                    &sync->pdos, data.pdo_pos))) {
        up(&master->master_sem);
        EC_ERR("Sync manager %u does not contain a Pdo with "
                "position %u in slave %u!\n", data.sync_index,
                data.pdo_pos, data.slave_position);
        return -EINVAL;
    }

    if (!(entry = ec_pdo_find_entry_by_pos_const(
                    pdo, data.entry_pos))) {
        up(&master->master_sem);
        EC_ERR("Pdo 0x%04X does not contain an entry with "
                "position %u in slave %u!\n", data.pdo_pos,
                data.entry_pos, data.slave_position);
        return -EINVAL;
    }

    data.index = entry->index;
    data.subindex = entry->subindex;
    data.bit_length = entry->bit_length;
    ec_cdev_strcpy(data.name, entry->name);

    up(&master->master_sem);

    if (copy_to_user((void __user *) arg, &data, sizeof(data)))
        return -EFAULT;

    return 0;
}

/*****************************************************************************/

/** Get domain information.
 */
int ec_cdev_ioctl_domain(
        ec_master_t *master, /**< EtherCAT master. */
        unsigned long arg /**< Userspace address to store the results. */
        )
{
    ec_ioctl_domain_t data;
    const ec_domain_t *domain;

    if (copy_from_user(&data, (void __user *) arg, sizeof(data))) {
        return -EFAULT;
    }

    if (down_interruptible(&master->master_sem))
        return -EINTR;

    if (!(domain = ec_master_find_domain_const(master, data.index))) {
        up(&master->master_sem);
        EC_ERR("Domain %u does not exist!\n", data.index);
        return -EINVAL;
    }

    data.data_size = domain->data_size;
    data.logical_base_address = domain->logical_base_address;
    data.working_counter = domain->working_counter;
    data.expected_working_counter = domain->expected_working_counter;
    data.fmmu_count = ec_domain_fmmu_count(domain);

    up(&master->master_sem);

    if (copy_to_user((void __user *) arg, &data, sizeof(data)))
        return -EFAULT;

    return 0;
}

/*****************************************************************************/

/** Get domain FMMU information.
 */
int ec_cdev_ioctl_domain_fmmu(
        ec_master_t *master, /**< EtherCAT master. */
        unsigned long arg /**< Userspace address to store the results. */
        )
{
    ec_ioctl_domain_fmmu_t data;
    const ec_domain_t *domain;
    const ec_fmmu_config_t *fmmu;

    if (copy_from_user(&data, (void __user *) arg, sizeof(data))) {
        return -EFAULT;
    }

    if (down_interruptible(&master->master_sem))
        return -EINTR;

    if (!(domain = ec_master_find_domain_const(master, data.domain_index))) {
        up(&master->master_sem);
        EC_ERR("Domain %u does not exist!\n", data.domain_index);
        return -EINVAL;
    }

    if (!(fmmu = ec_domain_find_fmmu(domain, data.fmmu_index))) {
        up(&master->master_sem);
        EC_ERR("Domain %u has less than %u fmmu configurations.\n",
                data.domain_index, data.fmmu_index + 1);
        return -EINVAL;
    }

    data.slave_config_alias = fmmu->sc->alias;
    data.slave_config_position = fmmu->sc->position;
    data.sync_index = fmmu->sync_index;
    data.dir = fmmu->dir;
    data.logical_address = fmmu->logical_start_address;
    data.data_size = fmmu->data_size;

    up(&master->master_sem);

    if (copy_to_user((void __user *) arg, &data, sizeof(data)))
        return -EFAULT;

    return 0;
}

/*****************************************************************************/

/** Get domain data.
 */
int ec_cdev_ioctl_domain_data(
        ec_master_t *master, /**< EtherCAT master. */
        unsigned long arg /**< Userspace address to store the results. */
        )
{
    ec_ioctl_domain_data_t data;
    const ec_domain_t *domain;

    if (copy_from_user(&data, (void __user *) arg, sizeof(data))) {
        return -EFAULT;
    }

    if (down_interruptible(&master->master_sem))
        return -EINTR;

    if (!(domain = ec_master_find_domain_const(master, data.domain_index))) {
        up(&master->master_sem);
        EC_ERR("Domain %u does not exist!\n", data.domain_index);
        return -EINVAL;
    }

    if (domain->data_size != data.data_size) {
        up(&master->master_sem);
        EC_ERR("Data size mismatch %u/%u!\n",
                data.data_size, domain->data_size);
        return -EFAULT;
    }

    if (copy_to_user((void __user *) data.target, domain->data,
                domain->data_size)) {
        up(&master->master_sem);
        return -EFAULT;
    }

    up(&master->master_sem);
    return 0;
}

/*****************************************************************************/

/** Set master debug level.
 */
int ec_cdev_ioctl_master_debug(
        ec_master_t *master, /**< EtherCAT master. */
        unsigned long arg /**< ioctl() argument. */
        )
{
    if (ec_master_debug_level(master, (unsigned int) arg))
        return -EINVAL;

    return 0;
}

/*****************************************************************************/

/** Set slave state.
 */
int ec_cdev_ioctl_slave_state(
        ec_master_t *master, /**< EtherCAT master. */
        unsigned long arg /**< ioctl() argument. */
        )
{
    ec_ioctl_slave_state_t data;
    ec_slave_t *slave;

    if (copy_from_user(&data, (void __user *) arg, sizeof(data))) {
        return -EFAULT;
    }

    if (down_interruptible(&master->master_sem))
        return -EINTR;

    if (!(slave = ec_master_find_slave(
                    master, 0, data.slave_position))) {
        up(&master->master_sem);
        EC_ERR("Slave %u does not exist!\n", data.slave_position);
        return -EINVAL;
    }

    ec_slave_request_state(slave, data.al_state);

    up(&master->master_sem);
    return 0;
}

/*****************************************************************************/

/** Get slave Sdo information.
 */
int ec_cdev_ioctl_slave_sdo(
        ec_master_t *master, /**< EtherCAT master. */
        unsigned long arg /**< ioctl() argument. */
        )
{
    ec_ioctl_slave_sdo_t data;
    const ec_slave_t *slave;
    const ec_sdo_t *sdo;

    if (copy_from_user(&data, (void __user *) arg, sizeof(data))) {
        return -EFAULT;
    }

    if (down_interruptible(&master->master_sem))
        return -EINTR;

    if (!(slave = ec_master_find_slave_const(
                    master, 0, data.slave_position))) {
        up(&master->master_sem);
        EC_ERR("Slave %u does not exist!\n", data.slave_position);
        return -EINVAL;
    }

    if (!(sdo = ec_slave_get_sdo_by_pos_const(
                    slave, data.sdo_position))) {
        up(&master->master_sem);
        EC_ERR("Sdo %u does not exist in slave %u!\n",
                data.sdo_position, data.slave_position);
        return -EINVAL;
    }

    data.sdo_index = sdo->index;
    data.max_subindex = sdo->max_subindex;
    ec_cdev_strcpy(data.name, sdo->name);

    up(&master->master_sem);

    if (copy_to_user((void __user *) arg, &data, sizeof(data)))
        return -EFAULT;

    return 0;
}

/*****************************************************************************/

/** Get slave Sdo entry information.
 */
int ec_cdev_ioctl_slave_sdo_entry(
        ec_master_t *master, /**< EtherCAT master. */
        unsigned long arg /**< ioctl() argument. */
        )
{
    ec_ioctl_slave_sdo_entry_t data;
    const ec_slave_t *slave;
    const ec_sdo_t *sdo;
    const ec_sdo_entry_t *entry;

    if (copy_from_user(&data, (void __user *) arg, sizeof(data))) {
        return -EFAULT;
    }

    if (down_interruptible(&master->master_sem))
        return -EINTR;

    if (!(slave = ec_master_find_slave_const(
                    master, 0, data.slave_position))) {
        up(&master->master_sem);
        EC_ERR("Slave %u does not exist!\n", data.slave_position);
        return -EINVAL;
    }

    if (data.sdo_spec <= 0) {
        if (!(sdo = ec_slave_get_sdo_by_pos_const(
                        slave, -data.sdo_spec))) {
            up(&master->master_sem);
            EC_ERR("Sdo %u does not exist in slave %u!\n",
                    -data.sdo_spec, data.slave_position);
            return -EINVAL;
        }
    } else {
        if (!(sdo = ec_slave_get_sdo_const(
                        slave, data.sdo_spec))) {
            up(&master->master_sem);
            EC_ERR("Sdo 0x%04X does not exist in slave %u!\n",
                    data.sdo_spec, data.slave_position);
            return -EINVAL;
        }
    }

    if (!(entry = ec_sdo_get_entry_const(
                    sdo, data.sdo_entry_subindex))) {
        up(&master->master_sem);
        EC_ERR("Sdo entry 0x%04X:%02X does not exist "
                "in slave %u!\n", sdo->index,
                data.sdo_entry_subindex, data.slave_position);
        return -EINVAL;
    }

    data.data_type = entry->data_type;
    data.bit_length = entry->bit_length;
    ec_cdev_strcpy(data.description, entry->description);

    up(&master->master_sem);

    if (copy_to_user((void __user *) arg, &data, sizeof(data)))
        return -EFAULT;

    return 0;
}

/*****************************************************************************/

/** Upload Sdo.
 */
int ec_cdev_ioctl_slave_sdo_upload(
        ec_master_t *master, /**< EtherCAT master. */
        unsigned long arg /**< ioctl() argument. */
        )
{
    ec_ioctl_slave_sdo_upload_t data;
    ec_master_sdo_request_t request;
    int retval;

    if (copy_from_user(&data, (void __user *) arg, sizeof(data))) {
        return -EFAULT;
    }

    ec_sdo_request_init(&request.req);
    ec_sdo_request_address(&request.req,
            data.sdo_index, data.sdo_entry_subindex);
    ecrt_sdo_request_read(&request.req);

    if (down_interruptible(&master->master_sem))
        return -EINTR;

    if (!(request.slave = ec_master_find_slave(
                    master, 0, data.slave_position))) {
        up(&master->master_sem);
        ec_sdo_request_clear(&request.req);
        EC_ERR("Slave %u does not exist!\n", data.slave_position);
        return -EINVAL;
    }

    // schedule request.
    list_add_tail(&request.list, &master->slave_sdo_requests);

    up(&master->master_sem);

    // wait for processing through FSM
    if (wait_event_interruptible(master->sdo_queue,
                request.req.state != EC_INT_REQUEST_QUEUED)) {
        // interrupted by signal
        down(&master->master_sem);
        if (request.req.state == EC_INT_REQUEST_QUEUED) {
            list_del(&request.req.list);
            up(&master->master_sem);
            ec_sdo_request_clear(&request.req);
            return -EINTR;
        }
        // request already processing: interrupt not possible.
        up(&master->master_sem);
    }

    // wait until master FSM has finished processing
    wait_event(master->sdo_queue, request.req.state != EC_INT_REQUEST_BUSY);

    data.abort_code = request.req.abort_code;

    if (request.req.state != EC_INT_REQUEST_SUCCESS) {
        data.data_size = 0;
        retval = -EIO;
    } else {
        if (request.req.data_size > data.target_size) {
            EC_ERR("Buffer too small.\n");
            ec_sdo_request_clear(&request.req);
            return -EOVERFLOW;
        }
        data.data_size = request.req.data_size;

        if (copy_to_user((void __user *) data.target,
                    request.req.data, data.data_size)) {
            ec_sdo_request_clear(&request.req);
            return -EFAULT;
        }
        retval = 0;
    }

    if (__copy_to_user((void __user *) arg, &data, sizeof(data))) {
        retval = -EFAULT;
    }

    ec_sdo_request_clear(&request.req);
    return retval;
}

/*****************************************************************************/

/** Download Sdo.
 */
int ec_cdev_ioctl_slave_sdo_download(
        ec_master_t *master, /**< EtherCAT master. */
        unsigned long arg /**< ioctl() argument. */
        )
{
    ec_ioctl_slave_sdo_download_t data;
    ec_master_sdo_request_t request;
    int retval;

    if (copy_from_user(&data, (void __user *) arg, sizeof(data))) {
        return -EFAULT;
    }

    // copy data to download
    if (!data.data_size) {
        EC_ERR("Zero data size!\n");
        return -EINVAL;
    }

    ec_sdo_request_init(&request.req);
    ec_sdo_request_address(&request.req,
            data.sdo_index, data.sdo_entry_subindex);
    if (ec_sdo_request_alloc(&request.req, data.data_size)) {
        ec_sdo_request_clear(&request.req);
        return -ENOMEM;
    }
    if (copy_from_user(request.req.data,
                (void __user *) data.data, data.data_size)) {
        ec_sdo_request_clear(&request.req);
        return -EFAULT;
    }
    request.req.data_size = data.data_size;
    ecrt_sdo_request_write(&request.req);

    if (down_interruptible(&master->master_sem))
        return -EINTR;

    if (!(request.slave = ec_master_find_slave(
                    master, 0, data.slave_position))) {
        up(&master->master_sem);
        EC_ERR("Slave %u does not exist!\n", data.slave_position);
        ec_sdo_request_clear(&request.req);
        return -EINVAL;
    }
    
    // schedule request.
    list_add_tail(&request.list, &master->slave_sdo_requests);

    up(&master->master_sem);

    // wait for processing through FSM
    if (wait_event_interruptible(master->sdo_queue,
                request.req.state != EC_INT_REQUEST_QUEUED)) {
        // interrupted by signal
        down(&master->master_sem);
        if (request.req.state == EC_INT_REQUEST_QUEUED) {
            list_del(&request.req.list);
            up(&master->master_sem);
            ec_sdo_request_clear(&request.req);
            return -EINTR;
        }
        // request already processing: interrupt not possible.
        up(&master->master_sem);
    }

    // wait until master FSM has finished processing
    wait_event(master->sdo_queue, request.req.state != EC_INT_REQUEST_BUSY);

    data.abort_code = request.req.abort_code;

    retval = request.req.state == EC_INT_REQUEST_SUCCESS ? 0 : -EIO;

    if (__copy_to_user((void __user *) arg, &data, sizeof(data))) {
        retval = -EFAULT;
    }

    ec_sdo_request_clear(&request.req);
    return retval;
}

/*****************************************************************************/

/** Read a slave's SII.
 */
int ec_cdev_ioctl_slave_sii_read(
        ec_master_t *master, /**< EtherCAT master. */
        unsigned long arg /**< ioctl() argument. */
        )
{
    ec_ioctl_slave_sii_t data;
    const ec_slave_t *slave;
    int retval;

    if (copy_from_user(&data, (void __user *) arg, sizeof(data))) {
        return -EFAULT;
    }

    if (down_interruptible(&master->master_sem))
        return -EINTR;

    if (!(slave = ec_master_find_slave_const(
                    master, 0, data.slave_position))) {
        up(&master->master_sem);
        EC_ERR("Slave %u does not exist!\n", data.slave_position);
        return -EINVAL;
    }

    if (!data.nwords
            || data.offset + data.nwords > slave->sii_nwords) {
        up(&master->master_sem);
        EC_ERR("Invalid SII read offset/size %u/%u for slave "
                "SII size %u!\n", data.offset,
                data.nwords, slave->sii_nwords);
        return -EINVAL;
    }

    if (copy_to_user((void __user *) data.words,
                slave->sii_words + data.offset, data.nwords * 2))
        retval = -EFAULT;
    else
        retval = 0;

    up(&master->master_sem);
    return retval;
}

/*****************************************************************************/

/** Write a slave's SII.
 */
int ec_cdev_ioctl_slave_sii_write(
        ec_master_t *master, /**< EtherCAT master. */
        unsigned long arg /**< ioctl() argument. */
        )
{
    ec_ioctl_slave_sii_t data;
    ec_slave_t *slave;
    unsigned int byte_size;
    uint16_t *words;
    ec_sii_write_request_t request;

    if (copy_from_user(&data, (void __user *) arg, sizeof(data))) {
        return -EFAULT;
    }

    if (!data.nwords)
        return 0;

    byte_size = sizeof(uint16_t) * data.nwords;
    if (!(words = kmalloc(byte_size, GFP_KERNEL))) {
        EC_ERR("Failed to allocate %u bytes for SII contents.\n",
                byte_size);
        return -ENOMEM;
    }

    if (copy_from_user(words,
                (void __user *) data.words, byte_size)) {
        kfree(words);
        return -EFAULT;
    }

    if (down_interruptible(&master->master_sem))
        return -EINTR;

    if (!(slave = ec_master_find_slave(
                    master, 0, data.slave_position))) {
        up(&master->master_sem);
        EC_ERR("Slave %u does not exist!\n", data.slave_position);
        kfree(words);
        return -EINVAL;
    }

    // init SII write request
    INIT_LIST_HEAD(&request.list);
    request.slave = slave;
    request.words = words;
    request.offset = data.offset;
    request.nwords = data.nwords;
    request.state = EC_INT_REQUEST_QUEUED;

    // schedule SII write request.
    list_add_tail(&request.list, &master->sii_requests);

    up(&master->master_sem);

    // wait for processing through FSM
    if (wait_event_interruptible(master->sii_queue,
                request.state != EC_INT_REQUEST_QUEUED)) {
        // interrupted by signal
        down(&master->master_sem);
        if (request.state == EC_INT_REQUEST_QUEUED) {
            // abort request
            list_del(&request.list);
            up(&master->master_sem);
            kfree(words);
            return -EINTR;
        }
        up(&master->master_sem);
    }

    // wait until master FSM has finished processing
    wait_event(master->sii_queue, request.state != EC_INT_REQUEST_BUSY);

    kfree(words);

    return request.state == EC_INT_REQUEST_SUCCESS ? 0 : -EIO;
}

/*****************************************************************************/

/** Read a slave's physical memory.
 */
int ec_cdev_ioctl_slave_phy_read(
        ec_master_t *master, /**< EtherCAT master. */
        unsigned long arg /**< ioctl() argument. */
        )
{
    ec_ioctl_slave_phy_t data;
    ec_slave_t *slave;
    uint8_t *contents;
    ec_phy_request_t request;

    if (copy_from_user(&data, (void __user *) arg, sizeof(data))) {
        return -EFAULT;
    }

    if (!data.length)
        return 0;

    if (!(contents = kmalloc(data.length, GFP_KERNEL))) {
        EC_ERR("Failed to allocate %u bytes for phy data.\n", data.length);
        return -ENOMEM;
    }

    if (down_interruptible(&master->master_sem))
        return -EINTR;

    if (!(slave = ec_master_find_slave(
                    master, 0, data.slave_position))) {
        up(&master->master_sem);
        EC_ERR("Slave %u does not exist!\n", data.slave_position);
        return -EINVAL;
    }

    // init phy request
    INIT_LIST_HEAD(&request.list);
    request.slave = slave;
    request.dir = EC_DIR_INPUT;
    request.data = contents;
    request.offset = data.offset;
    request.length = data.length;
    request.state = EC_INT_REQUEST_QUEUED;

    // schedule request.
    list_add_tail(&request.list, &master->phy_requests);

    up(&master->master_sem);

    // wait for processing through FSM
    if (wait_event_interruptible(master->phy_queue,
                request.state != EC_INT_REQUEST_QUEUED)) {
        // interrupted by signal
        down(&master->master_sem);
        if (request.state == EC_INT_REQUEST_QUEUED) {
            // abort request
            list_del(&request.list);
            up(&master->master_sem);
            kfree(contents);
            return -EINTR;
        }
        up(&master->master_sem);
    }

    // wait until master FSM has finished processing
    wait_event(master->phy_queue, request.state != EC_INT_REQUEST_BUSY);

    if (request.state == EC_INT_REQUEST_SUCCESS) {
        if (copy_to_user((void __user *) data.data, contents, data.length))
            return -EFAULT;
    }
    kfree(contents);

    return request.state == EC_INT_REQUEST_SUCCESS ? 0 : -EIO;
}

/*****************************************************************************/

/** Write a slave's physical memory.
 */
int ec_cdev_ioctl_slave_phy_write(
        ec_master_t *master, /**< EtherCAT master. */
        unsigned long arg /**< ioctl() argument. */
        )
{
    ec_ioctl_slave_phy_t data;
    ec_slave_t *slave;
    uint8_t *contents;
    ec_phy_request_t request;

    if (copy_from_user(&data, (void __user *) arg, sizeof(data))) {
        return -EFAULT;
    }

    if (!data.length)
        return 0;

    if (!(contents = kmalloc(data.length, GFP_KERNEL))) {
        EC_ERR("Failed to allocate %u bytes for phy data.\n", data.length);
        return -ENOMEM;
    }

    if (copy_from_user(contents, (void __user *) data.data, data.length)) {
        kfree(contents);
        return -EFAULT;
    }

    if (down_interruptible(&master->master_sem))
        return -EINTR;

    if (!(slave = ec_master_find_slave(
                    master, 0, data.slave_position))) {
        up(&master->master_sem);
        EC_ERR("Slave %u does not exist!\n", data.slave_position);
        kfree(contents);
        return -EINVAL;
    }

    // init phy request
    INIT_LIST_HEAD(&request.list);
    request.slave = slave;
    request.dir = EC_DIR_OUTPUT;
    request.data = contents;
    request.offset = data.offset;
    request.length = data.length;
    request.state = EC_INT_REQUEST_QUEUED;

    // schedule request.
    list_add_tail(&request.list, &master->phy_requests);

    up(&master->master_sem);

    // wait for processing through FSM
    if (wait_event_interruptible(master->phy_queue,
                request.state != EC_INT_REQUEST_QUEUED)) {
        // interrupted by signal
        down(&master->master_sem);
        if (request.state == EC_INT_REQUEST_QUEUED) {
            // abort request
            list_del(&request.list);
            up(&master->master_sem);
            kfree(contents);
            return -EINTR;
        }
        up(&master->master_sem);
    }

    // wait until master FSM has finished processing
    wait_event(master->phy_queue, request.state != EC_INT_REQUEST_BUSY);

    kfree(contents);

    return request.state == EC_INT_REQUEST_SUCCESS ? 0 : -EIO;
}

/*****************************************************************************/

/** Get slave configuration information.
 */
int ec_cdev_ioctl_config(
        ec_master_t *master, /**< EtherCAT master. */
        unsigned long arg /**< ioctl() argument. */
        )
{
    ec_ioctl_config_t data;
    const ec_slave_config_t *sc;
    uint8_t i;

    if (copy_from_user(&data, (void __user *) arg, sizeof(data))) {
        return -EFAULT;
    }

    if (down_interruptible(&master->master_sem))
        return -EINTR;

    if (!(sc = ec_master_get_config_const(
                    master, data.config_index))) {
        up(&master->master_sem);
        EC_ERR("Slave config %u does not exist!\n",
                data.config_index);
        return -EINVAL;
    }

    data.alias = sc->alias;
    data.position = sc->position;
    data.vendor_id = sc->vendor_id;
    data.product_code = sc->product_code;
    for (i = 0; i < EC_MAX_SYNC_MANAGERS; i++) {
        data.syncs[i].dir = sc->sync_configs[i].dir;
        data.syncs[i].pdo_count =
            ec_pdo_list_count(&sc->sync_configs[i].pdos);
    }
    data.sdo_count = ec_slave_config_sdo_count(sc);
    data.slave_position = sc->slave ? sc->slave->ring_position : -1;

    up(&master->master_sem);

    if (copy_to_user((void __user *) arg, &data, sizeof(data)))
        return -EFAULT;

    return 0;
}

/*****************************************************************************/

/** Get slave configuration Pdo information.
 */
int ec_cdev_ioctl_config_pdo(
        ec_master_t *master, /**< EtherCAT master. */
        unsigned long arg /**< ioctl() argument. */
        )
{
    ec_ioctl_config_pdo_t data;
    const ec_slave_config_t *sc;
    const ec_pdo_t *pdo;

    if (copy_from_user(&data, (void __user *) arg, sizeof(data))) {
        return -EFAULT;
    }

    if (data.sync_index >= EC_MAX_SYNC_MANAGERS) {
        EC_ERR("Invalid sync manager index %u!\n",
                data.sync_index);
        return -EINVAL;
    }

    if (down_interruptible(&master->master_sem))
        return -EINTR;

    if (!(sc = ec_master_get_config_const(
                    master, data.config_index))) {
        up(&master->master_sem);
        EC_ERR("Slave config %u does not exist!\n",
                data.config_index);
        return -EINVAL;
    }

    if (!(pdo = ec_pdo_list_find_pdo_by_pos_const(
                    &sc->sync_configs[data.sync_index].pdos,
                    data.pdo_pos))) {
        up(&master->master_sem);
        EC_ERR("Invalid Pdo position!\n");
        return -EINVAL;
    }

    data.index = pdo->index;
    data.entry_count = ec_pdo_entry_count(pdo);
    ec_cdev_strcpy(data.name, pdo->name);

    up(&master->master_sem);

    if (copy_to_user((void __user *) arg, &data, sizeof(data)))
        return -EFAULT;

    return 0;
}

/*****************************************************************************/

/** Get slave configuration Pdo entry information.
 */
int ec_cdev_ioctl_config_pdo_entry(
        ec_master_t *master, /**< EtherCAT master. */
        unsigned long arg /**< ioctl() argument. */
        )
{
    ec_ioctl_config_pdo_entry_t data;
    const ec_slave_config_t *sc;
    const ec_pdo_t *pdo;
    const ec_pdo_entry_t *entry;

    if (copy_from_user(&data, (void __user *) arg, sizeof(data))) {
        return -EFAULT;
    }

    if (data.sync_index >= EC_MAX_SYNC_MANAGERS) {
        EC_ERR("Invalid sync manager index %u!\n",
                data.sync_index);
        return -EINVAL;
    }

    if (down_interruptible(&master->master_sem))
        return -EINTR;

    if (!(sc = ec_master_get_config_const(
                    master, data.config_index))) {
        up(&master->master_sem);
        EC_ERR("Slave config %u does not exist!\n",
                data.config_index);
        return -EINVAL;
    }

    if (!(pdo = ec_pdo_list_find_pdo_by_pos_const(
                    &sc->sync_configs[data.sync_index].pdos,
                    data.pdo_pos))) {
        up(&master->master_sem);
        EC_ERR("Invalid Pdo position!\n");
        return -EINVAL;
    }

    if (!(entry = ec_pdo_find_entry_by_pos_const(
                    pdo, data.entry_pos))) {
        up(&master->master_sem);
        EC_ERR("Entry not found!\n");
        return -EINVAL;
    }

    data.index = entry->index;
    data.subindex = entry->subindex;
    data.bit_length = entry->bit_length;
    ec_cdev_strcpy(data.name, entry->name);

    up(&master->master_sem);

    if (copy_to_user((void __user *) arg, &data, sizeof(data)))
        return -EFAULT;

    return 0;
}

/*****************************************************************************/

/** Get slave configuration Sdo information.
 */
int ec_cdev_ioctl_config_sdo(
        ec_master_t *master, /**< EtherCAT master. */
        unsigned long arg /**< ioctl() argument. */
        )
{
    ec_ioctl_config_sdo_t data;
    const ec_slave_config_t *sc;
    const ec_sdo_request_t *req;

    if (copy_from_user(&data, (void __user *) arg, sizeof(data))) {
        return -EFAULT;
    }

    if (down_interruptible(&master->master_sem))
        return -EINTR;

    if (!(sc = ec_master_get_config_const(
                    master, data.config_index))) {
        up(&master->master_sem);
        EC_ERR("Slave config %u does not exist!\n",
                data.config_index);
        return -EINVAL;
    }

    if (!(req = ec_slave_config_get_sdo_by_pos_const(
                    sc, data.sdo_pos))) {
        up(&master->master_sem);
        EC_ERR("Invalid Sdo position!\n");
        return -EINVAL;
    }

    data.index = req->index;
    data.subindex = req->subindex;
    data.size = req->data_size;
    memcpy(&data.data, req->data, min((u32) data.size, (u32) 4));

    up(&master->master_sem);

    if (copy_to_user((void __user *) arg, &data, sizeof(data)))
        return -EFAULT;

    return 0;
}

/*****************************************************************************/

/** Request the master from userspace.
 */
int ec_cdev_ioctl_request(
        ec_master_t *master, /**< EtherCAT master. */
        unsigned long arg, /**< ioctl() argument. */
        ec_cdev_priv_t *priv /**< Private data structure of file handle. */
        )
{
	ec_master_t *m;
    int ret = 0;

    m = ecrt_request_master(master->index);
    if (IS_ERR(m)) {
        ret = PTR_ERR(m);
    } else {
        priv->requested = 1;
    }

    return ret;
}

/*****************************************************************************/

/** Create a domain.
 */
int ec_cdev_ioctl_create_domain(
        ec_master_t *master, /**< EtherCAT master. */
        unsigned long arg, /**< ioctl() argument. */
        ec_cdev_priv_t *priv /**< Private data structure of file handle. */
        )
{
    ec_domain_t *domain;

	if (unlikely(!priv->requested))
		return -EPERM;

    domain = ecrt_master_create_domain(master);
    if (!domain)
        return -ENOMEM;

    return domain->index;
}

/*****************************************************************************/

/** Create a slave configuration.
 */
int ec_cdev_ioctl_create_slave_config(
        ec_master_t *master, /**< EtherCAT master. */
        unsigned long arg, /**< ioctl() argument. */
        ec_cdev_priv_t *priv /**< Private data structure of file handle. */
        )
{
    ec_ioctl_config_t data;
    ec_slave_config_t *sc, *entry;

	if (unlikely(!priv->requested))
		return -EPERM;

    if (copy_from_user(&data, (void __user *) arg, sizeof(data))) {
        return -EFAULT;
    }

    sc = ecrt_master_slave_config(master, data.alias, data.position,
            data.vendor_id, data.product_code);
    if (!sc)
        return -ENODEV; // FIXME

    data.config_index = 0;

    if (down_interruptible(&master->master_sem))
        return -EINTR;

    list_for_each_entry(entry, &master->configs, list) {
        if (entry == sc)
            break;
        data.config_index++;
    }

    up(&master->master_sem);

    if (copy_to_user((void __user *) arg, &data, sizeof(data)))
        return -EFAULT;

    return 0;
}

/*****************************************************************************/

/** Activates the master.
 */
int ec_cdev_ioctl_activate(
        ec_master_t *master, /**< EtherCAT master. */
        unsigned long arg, /**< ioctl() argument. */
        ec_cdev_priv_t *priv /**< Private data structure of file handle. */
        )
{
    ec_domain_t *domain;
    off_t offset;
    
	if (unlikely(!priv->requested))
		return -EPERM;

    /* Get the sum of the domains' process data sizes. */
    
    priv->process_data_size = 0;

    if (down_interruptible(&master->master_sem))
        return -EINTR;

    list_for_each_entry(domain, &master->domains, list) {
        priv->process_data_size += ecrt_domain_size(domain);
    }
    
    up(&master->master_sem);

    if (priv->process_data_size) {
        priv->process_data = vmalloc(priv->process_data_size);
        if (!priv->process_data) {
            priv->process_data_size = 0;
            return -ENOMEM;
        }

        /* Set the memory as external process data memory for the domains. */

        offset = 0;
        list_for_each_entry(domain, &master->domains, list) {
            ecrt_domain_external_memory(domain, priv->process_data + offset);
            offset += ecrt_domain_size(domain);
        }
    }

    if (ecrt_master_activate(master))
        return -EIO;

    if (copy_to_user((void __user *) arg,
                &priv->process_data_size, sizeof(size_t)))
        return -EFAULT;

    return 0;
}

/*****************************************************************************/

/** Send frames.
 */
int ec_cdev_ioctl_send(
        ec_master_t *master, /**< EtherCAT master. */
        unsigned long arg, /**< ioctl() argument. */
        ec_cdev_priv_t *priv /**< Private data structure of file handle. */
        )
{
	if (unlikely(!priv->requested))
		return -EPERM;

    spin_lock_bh(&master->internal_lock);
    ecrt_master_send(master);
    spin_unlock_bh(&master->internal_lock);
    return 0;
}

/*****************************************************************************/

/** Receive frames.
 */
int ec_cdev_ioctl_receive(
        ec_master_t *master, /**< EtherCAT master. */
        unsigned long arg, /**< ioctl() argument. */
        ec_cdev_priv_t *priv /**< Private data structure of file handle. */
        )
{
	if (unlikely(!priv->requested))
		return -EPERM;

    spin_lock_bh(&master->internal_lock);
    ecrt_master_receive(master);
    spin_unlock_bh(&master->internal_lock);
    return 0;
}

/*****************************************************************************/

/** Set the direction of a sync manager.
 */
int ec_cdev_ioctl_sc_sync(
        ec_master_t *master, /**< EtherCAT master. */
        unsigned long arg, /**< ioctl() argument. */
        ec_cdev_priv_t *priv /**< Private data structure of file handle. */
        )
{
    ec_ioctl_config_t data;
    ec_slave_config_t *sc;
    unsigned int i;
    int ret = 0;

	if (unlikely(!priv->requested)) {
        ret = -EPERM;
        goto out_return;
    }

    if (copy_from_user(&data, (void __user *) arg, sizeof(data))) {
        ret = -EFAULT;
        goto out_return;
    }

    if (down_interruptible(&master->master_sem)) {
        ret = -EINTR;
        goto out_return;
    }

    if (!(sc = ec_master_get_config(master, data.config_index))) {
        ret = -ESRCH;
        goto out_up;
    }

    for (i = 0; i < EC_MAX_SYNC_MANAGERS; i++) {
        ec_direction_t dir = data.syncs[i].dir;
        if (dir == EC_DIR_INPUT || dir == EC_DIR_OUTPUT) {
            if (ecrt_slave_config_sync_manager(sc, i, dir)) {
                ret = -EINVAL;
                goto out_up;
            }
        }
    }

out_up:
    up(&master->master_sem);
out_return:
    return ret;
}

/*****************************************************************************/

/** Add a Pdo to the assignment.
 */
int ec_cdev_ioctl_sc_add_pdo(
        ec_master_t *master, /**< EtherCAT master. */
        unsigned long arg, /**< ioctl() argument. */
        ec_cdev_priv_t *priv /**< Private data structure of file handle. */
        )
{
    ec_ioctl_config_pdo_t data;
    ec_slave_config_t *sc;

	if (unlikely(!priv->requested))
        return -EPERM;

    if (copy_from_user(&data, (void __user *) arg, sizeof(data)))
        return -EFAULT;

    if (down_interruptible(&master->master_sem))
        return -EINTR;

    if (!(sc = ec_master_get_config(master, data.config_index))) {
        up(&master->master_sem);
        return -ESRCH;
    }

    up(&master->master_sem); // FIXME

    return ecrt_slave_config_pdo_assign_add(sc, data.sync_index, data.index);
}

/*****************************************************************************/

/** Clears the Pdo assignment.
 */
int ec_cdev_ioctl_sc_clear_pdos(
        ec_master_t *master, /**< EtherCAT master. */
        unsigned long arg, /**< ioctl() argument. */
        ec_cdev_priv_t *priv /**< Private data structure of file handle. */
        )
{
    ec_ioctl_config_pdo_t data;
    ec_slave_config_t *sc;

	if (unlikely(!priv->requested))
        return -EPERM;

    if (copy_from_user(&data, (void __user *) arg, sizeof(data)))
        return -EFAULT;

    if (down_interruptible(&master->master_sem))
        return -EINTR;

    if (!(sc = ec_master_get_config(master, data.config_index))) {
        up(&master->master_sem);
        return -ESRCH;
    }

    up(&master->master_sem); // FIXME

    ecrt_slave_config_pdo_assign_clear(sc, data.sync_index);
    return 0;
}

/*****************************************************************************/

/** Add an entry to a Pdo's mapping.
 */
int ec_cdev_ioctl_sc_add_entry(
        ec_master_t *master, /**< EtherCAT master. */
        unsigned long arg, /**< ioctl() argument. */
        ec_cdev_priv_t *priv /**< Private data structure of file handle. */
        )
{
    ec_ioctl_add_pdo_entry_t data;
    ec_slave_config_t *sc;

	if (unlikely(!priv->requested))
        return -EPERM;

    if (copy_from_user(&data, (void __user *) arg, sizeof(data)))
        return -EFAULT;

    if (down_interruptible(&master->master_sem))
        return -EINTR;

    if (!(sc = ec_master_get_config(master, data.config_index))) {
        up(&master->master_sem);
        return -ESRCH;
    }

    up(&master->master_sem); // FIXME

    return ecrt_slave_config_pdo_mapping_add(sc, data.pdo_index,
            data.entry_index, data.entry_subindex, data.entry_bit_length);
}

/*****************************************************************************/

/** Clears the mapping of a Pdo.
 */
int ec_cdev_ioctl_sc_clear_entries(
        ec_master_t *master, /**< EtherCAT master. */
        unsigned long arg, /**< ioctl() argument. */
        ec_cdev_priv_t *priv /**< Private data structure of file handle. */
        )
{
    ec_ioctl_config_pdo_t data;
    ec_slave_config_t *sc;

	if (unlikely(!priv->requested))
        return -EPERM;

    if (copy_from_user(&data, (void __user *) arg, sizeof(data)))
        return -EFAULT;

    if (down_interruptible(&master->master_sem))
        return -EINTR;

    if (!(sc = ec_master_get_config(master, data.config_index))) {
        up(&master->master_sem);
        return -ESRCH;
    }

    up(&master->master_sem); // FIXME

    ecrt_slave_config_pdo_mapping_clear(sc, data.index);
    return 0;
}

/*****************************************************************************/

/** Registers a Pdo entry.
 */
int ec_cdev_ioctl_sc_reg_pdo_entry(
        ec_master_t *master, /**< EtherCAT master. */
        unsigned long arg, /**< ioctl() argument. */
        ec_cdev_priv_t *priv /**< Private data structure of file handle. */
        )
{
    ec_ioctl_reg_pdo_entry_t data;
    ec_slave_config_t *sc;
    ec_domain_t *domain;
    int ret;

	if (unlikely(!priv->requested))
        return -EPERM;

    if (copy_from_user(&data, (void __user *) arg, sizeof(data)))
        return -EFAULT;

    if (down_interruptible(&master->master_sem))
        return -EINTR;

    if (!(sc = ec_master_get_config(master, data.config_index))) {
        up(&master->master_sem);
        return -ESRCH;
    }

    if (!(domain = ec_master_find_domain(master, data.domain_index))) {
        up(&master->master_sem);
        return -ESRCH;
    }

    up(&master->master_sem); // FIXME

    ret = ecrt_slave_config_reg_pdo_entry(sc, data.entry_index,
            data.entry_subindex, domain, &data.bit_position);

    if (copy_to_user((void __user *) arg, &data, sizeof(data)))
        return -EFAULT;

    return ret;
}

/*****************************************************************************/

/** Configures an Sdo.
 */
int ec_cdev_ioctl_sc_sdo(
        ec_master_t *master, /**< EtherCAT master. */
        unsigned long arg, /**< ioctl() argument. */
        ec_cdev_priv_t *priv /**< Private data structure of file handle. */
        )
{
    ec_ioctl_sc_sdo_t data;
    ec_slave_config_t *sc;
    uint8_t *sdo_data = NULL;
    int ret;

	if (unlikely(!priv->requested))
        return -EPERM;

    if (copy_from_user(&data, (void __user *) arg, sizeof(data)))
        return -EFAULT;

    if (!data.size)
        return -EINVAL;

    if (!(sdo_data = kmalloc(data.size, GFP_KERNEL))) {
        return -ENOMEM;
    }

    if (copy_from_user(sdo_data, (void __user *) data.data, data.size)) {
        kfree(sdo_data);
        return -EFAULT;
    }

    if (down_interruptible(&master->master_sem)) {
        kfree(sdo_data);
        return -EINTR;
    }

    if (!(sc = ec_master_get_config(master, data.config_index))) {
        up(&master->master_sem);
        kfree(sdo_data);
        return -ESRCH;
    }

    up(&master->master_sem); // FIXME

    ret = ecrt_slave_config_sdo(sc, data.index, data.subindex, sdo_data,
            data.size);
    kfree(sdo_data);
    return ret;
}

/*****************************************************************************/

/** Gets the domain's offset in the total process data.
 */
int ec_cdev_ioctl_domain_offset(
        ec_master_t *master, /**< EtherCAT master. */
        unsigned long arg, /**< ioctl() argument. */
        ec_cdev_priv_t *priv /**< Private data structure of file handle. */
        )
{
    int offset = 0;
    const ec_domain_t *domain;

	if (unlikely(!priv->requested))
        return -EPERM;

    if (down_interruptible(&master->master_sem)) {
        return -EINTR;
    }

    list_for_each_entry(domain, &master->domains, list) {
        if (domain->index == arg) {
            up(&master->master_sem);
            return offset;
        }
        offset += ecrt_domain_size(domain);
    }

    up(&master->master_sem);
    return -ESRCH;
}

/*****************************************************************************/

/** Process the domain.
 */
int ec_cdev_ioctl_domain_process(
        ec_master_t *master, /**< EtherCAT master. */
        unsigned long arg, /**< ioctl() argument. */
        ec_cdev_priv_t *priv /**< Private data structure of file handle. */
        )
{
    ec_domain_t *domain;

	if (unlikely(!priv->requested))
        return -EPERM;

    if (down_interruptible(&master->master_sem))
        return -EINTR;

    if (!(domain = ec_master_find_domain(master, arg))) {
        up(&master->master_sem);
        return -ESRCH;
    }

    ecrt_domain_process(domain);
    up(&master->master_sem);
    return 0;
}

/*****************************************************************************/

/** Queue the domain.
 */
int ec_cdev_ioctl_domain_queue(
        ec_master_t *master, /**< EtherCAT master. */
        unsigned long arg, /**< ioctl() argument. */
        ec_cdev_priv_t *priv /**< Private data structure of file handle. */
        )
{
    ec_domain_t *domain;

	if (unlikely(!priv->requested))
        return -EPERM;

    if (down_interruptible(&master->master_sem))
        return -EINTR;

    if (!(domain = ec_master_find_domain(master, arg))) {
        up(&master->master_sem);
        return -ESRCH;
    }

    ecrt_domain_queue(domain);
    up(&master->master_sem);
    return 0;
}

/******************************************************************************
 * File operations
 *****************************************************************************/

/** Called when the cdev is opened.
 */
int eccdev_open(struct inode *inode, struct file *filp)
{
    ec_cdev_t *cdev = container_of(inode->i_cdev, ec_cdev_t, cdev);
    ec_master_t *master = cdev->master;
    ec_cdev_priv_t *priv;

    priv = kmalloc(sizeof(ec_cdev_priv_t), GFP_KERNEL);
    if (!priv) {
        EC_ERR("Failed to allocate memory for private data structure.\n");
        return -ENOMEM;
    }

    priv->cdev = cdev;
    priv->requested = 0;
    priv->process_data = NULL;
    priv->process_data_size = 0;

    filp->private_data = priv;
    if (master->debug_level)
        EC_DBG("File opened.\n");
    return 0;
}

/*****************************************************************************/

/** Called when the cdev is closed.
 */
int eccdev_release(struct inode *inode, struct file *filp)
{
    ec_cdev_priv_t *priv = (ec_cdev_priv_t *) filp->private_data;
    ec_master_t *master = priv->cdev->master;

    if (priv->requested)
        ecrt_release_master(master);

    if (priv->process_data)
        vfree(priv->process_data);

    if (master->debug_level)
        EC_DBG("File closed.\n");
    kfree(priv);
    return 0;
}

/*****************************************************************************/

/** Called when an ioctl() command is issued.
 */
long eccdev_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    ec_cdev_priv_t *priv = (ec_cdev_priv_t *) filp->private_data;
    ec_master_t *master = priv->cdev->master;

    if (master->debug_level)
        EC_DBG("ioctl(filp = %x, cmd = %u (%u), arg = %x)\n",
                (u32) filp, (u32) cmd, (u32) _IOC_NR(cmd), (u32) arg);

    switch (cmd) {
        case EC_IOCTL_MASTER:
            return ec_cdev_ioctl_master(master, arg);
        case EC_IOCTL_SLAVE:
            return ec_cdev_ioctl_slave(master, arg);
        case EC_IOCTL_SLAVE_SYNC:
            return ec_cdev_ioctl_slave_sync(master, arg);
        case EC_IOCTL_SLAVE_SYNC_PDO:
            return ec_cdev_ioctl_slave_sync_pdo(master, arg);
        case EC_IOCTL_SLAVE_SYNC_PDO_ENTRY:
            return ec_cdev_ioctl_slave_sync_pdo_entry(master, arg);
        case EC_IOCTL_DOMAIN:
            return ec_cdev_ioctl_domain(master, arg);
        case EC_IOCTL_DOMAIN_FMMU:
            return ec_cdev_ioctl_domain_fmmu(master, arg);
        case EC_IOCTL_DOMAIN_DATA:
            return ec_cdev_ioctl_domain_data(master, arg);
        case EC_IOCTL_MASTER_DEBUG:
            if (!(filp->f_mode & FMODE_WRITE))
                return -EPERM;
            return ec_cdev_ioctl_master_debug(master, arg);
        case EC_IOCTL_SLAVE_STATE:
            if (!(filp->f_mode & FMODE_WRITE))
                return -EPERM;
            return ec_cdev_ioctl_slave_state(master, arg);
        case EC_IOCTL_SLAVE_SDO:
            return ec_cdev_ioctl_slave_sdo(master, arg);
        case EC_IOCTL_SLAVE_SDO_ENTRY:
            return ec_cdev_ioctl_slave_sdo_entry(master, arg);
        case EC_IOCTL_SLAVE_SDO_UPLOAD:
            return ec_cdev_ioctl_slave_sdo_upload(master, arg);
        case EC_IOCTL_SLAVE_SDO_DOWNLOAD:
            if (!(filp->f_mode & FMODE_WRITE))
                return -EPERM;
            return ec_cdev_ioctl_slave_sdo_download(master, arg);
        case EC_IOCTL_SLAVE_SII_READ:
            return ec_cdev_ioctl_slave_sii_read(master, arg);
        case EC_IOCTL_SLAVE_SII_WRITE:
            if (!(filp->f_mode & FMODE_WRITE))
                return -EPERM;
            return ec_cdev_ioctl_slave_sii_write(master, arg);
        case EC_IOCTL_SLAVE_PHY_READ:
            return ec_cdev_ioctl_slave_phy_read(master, arg);
        case EC_IOCTL_SLAVE_PHY_WRITE:
            if (!(filp->f_mode & FMODE_WRITE))
                return -EPERM;
            return ec_cdev_ioctl_slave_phy_write(master, arg);
        case EC_IOCTL_CONFIG:
            return ec_cdev_ioctl_config(master, arg);
        case EC_IOCTL_CONFIG_PDO:
            return ec_cdev_ioctl_config_pdo(master, arg);
        case EC_IOCTL_CONFIG_PDO_ENTRY:
            return ec_cdev_ioctl_config_pdo_entry(master, arg);
        case EC_IOCTL_CONFIG_SDO:
            return ec_cdev_ioctl_config_sdo(master, arg);
        case EC_IOCTL_REQUEST:
            if (!(filp->f_mode & FMODE_WRITE))
				return -EPERM;
			return ec_cdev_ioctl_request(master, arg, priv);
        case EC_IOCTL_CREATE_DOMAIN:
            if (!(filp->f_mode & FMODE_WRITE))
				return -EPERM;
			return ec_cdev_ioctl_create_domain(master, arg, priv);
        case EC_IOCTL_CREATE_SLAVE_CONFIG:
            if (!(filp->f_mode & FMODE_WRITE))
				return -EPERM;
			return ec_cdev_ioctl_create_slave_config(master, arg, priv);
        case EC_IOCTL_ACTIVATE:
            if (!(filp->f_mode & FMODE_WRITE))
				return -EPERM;
			return ec_cdev_ioctl_activate(master, arg, priv);
        case EC_IOCTL_SEND:
            if (!(filp->f_mode & FMODE_WRITE))
				return -EPERM;
			return ec_cdev_ioctl_send(master, arg, priv);
        case EC_IOCTL_RECEIVE:
            if (!(filp->f_mode & FMODE_WRITE))
				return -EPERM;
			return ec_cdev_ioctl_receive(master, arg, priv);
        case EC_IOCTL_SC_SYNC:
            if (!(filp->f_mode & FMODE_WRITE))
				return -EPERM;
			return ec_cdev_ioctl_sc_sync(master, arg, priv);
        case EC_IOCTL_SC_ADD_PDO:
            if (!(filp->f_mode & FMODE_WRITE))
				return -EPERM;
			return ec_cdev_ioctl_sc_add_pdo(master, arg, priv);
        case EC_IOCTL_SC_CLEAR_PDOS:
            if (!(filp->f_mode & FMODE_WRITE))
				return -EPERM;
			return ec_cdev_ioctl_sc_clear_pdos(master, arg, priv);
        case EC_IOCTL_SC_ADD_ENTRY:
            if (!(filp->f_mode & FMODE_WRITE))
				return -EPERM;
			return ec_cdev_ioctl_sc_add_entry(master, arg, priv);
        case EC_IOCTL_SC_CLEAR_ENTRIES:
            if (!(filp->f_mode & FMODE_WRITE))
				return -EPERM;
			return ec_cdev_ioctl_sc_clear_entries(master, arg, priv);
        case EC_IOCTL_SC_REG_PDO_ENTRY:
            if (!(filp->f_mode & FMODE_WRITE))
				return -EPERM;
			return ec_cdev_ioctl_sc_reg_pdo_entry(master, arg, priv);
        case EC_IOCTL_SC_SDO:
            if (!(filp->f_mode & FMODE_WRITE))
				return -EPERM;
			return ec_cdev_ioctl_sc_sdo(master, arg, priv);
        case EC_IOCTL_DOMAIN_OFFSET:
			return ec_cdev_ioctl_domain_offset(master, arg, priv);
        case EC_IOCTL_DOMAIN_PROCESS:
            if (!(filp->f_mode & FMODE_WRITE))
				return -EPERM;
			return ec_cdev_ioctl_domain_process(master, arg, priv);
        case EC_IOCTL_DOMAIN_QUEUE:
            if (!(filp->f_mode & FMODE_WRITE))
				return -EPERM;
			return ec_cdev_ioctl_domain_queue(master, arg, priv);
        default:
            return -ENOTTY;
    }
}

/*****************************************************************************/

int eccdev_mmap(
        struct file *filp,
        struct vm_area_struct *vma
        )
{
    ec_cdev_priv_t *priv = (ec_cdev_priv_t *) filp->private_data;

    if (priv->cdev->master->debug_level)
        EC_DBG("mmap()\n");

    vma->vm_ops = &eccdev_vm_ops;
    vma->vm_flags |= VM_RESERVED; /* Pages will not be swapped out */
    vma->vm_private_data = priv;

    return 0;
}

/*****************************************************************************/

struct page *eccdev_vma_nopage(
        struct vm_area_struct *vma,
        unsigned long address,
        int *type
        )
{
    unsigned long offset;
    struct page *page = NOPAGE_SIGBUS;
    ec_cdev_priv_t *priv = (ec_cdev_priv_t *) vma->vm_private_data;

    offset = (address - vma->vm_start) + (vma->vm_pgoff << PAGE_SHIFT);

    if (offset >= priv->process_data_size)
        return NOPAGE_SIGBUS;

    page = vmalloc_to_page(priv->process_data + offset);

    if (priv->cdev->master->debug_level)
        EC_DBG("Nopage fault vma, address = %#lx, offset = %#lx, page = %p\n",
                address, offset, page);

    get_page(page);
    if (type)
        *type = VM_FAULT_MINOR;

    return page;
}

/*****************************************************************************/
