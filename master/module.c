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
   EtherCAT master driver module.
*/

/*****************************************************************************/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>

#include "globals.h"
#include "master.h"
#include "device.h"
#include "xmldev.h"

/*****************************************************************************/

#define MAX_MASTERS 5 /**< maximum number of masters */

/*****************************************************************************/

int __init ec_init_module(void);
void __exit ec_cleanup_module(void);

static int ec_mac_parse(uint8_t *, const char *, int);

/*****************************************************************************/

struct kobject kobj; /**< kobject for master module */

static char *main[MAX_MASTERS]; /**< main devices parameter */
static char *backup[MAX_MASTERS]; /**< backup devices parameter */

static ec_master_t *masters; /**< master array */
static struct semaphore master_sem; /**< master semaphore */
static unsigned int master_count; /**< number of masters */
static unsigned int backup_count; /**< number of backup devices */

static uint8_t macs[MAX_MASTERS][2][ETH_ALEN]; /**< MAC addresses */

static dev_t device_number; /**< XML character device number */
ec_xmldev_t xmldev; /**< XML character device */

char *ec_master_version_str = EC_MASTER_VERSION;

/*****************************************************************************/

/** \cond */

MODULE_AUTHOR("Florian Pose <fp@igh-essen.com>");
MODULE_DESCRIPTION("EtherCAT master driver module");
MODULE_LICENSE("GPL");
MODULE_VERSION(EC_MASTER_VERSION);

module_param_array(main, charp, &master_count, S_IRUGO);
MODULE_PARM_DESC(main, "MAC addresses of main devices");
module_param_array(backup, charp, &backup_count, S_IRUGO);
MODULE_PARM_DESC(backup, "MAC addresses of backup devices");

/** \endcond */

/*****************************************************************************/

/**
 * Module initialization.
 * Initializes \a ec_master_count masters.
 * \return 0 on success, else < 0
 */

int __init ec_init_module(void)
{
    int i, ret = 0;

    EC_INFO("Master driver %s\n", EC_MASTER_VERSION);

    init_MUTEX(&master_sem);

    // init kobject and add it to the hierarchy
    memset(&kobj, 0x00, sizeof(struct kobject));
    kobject_init(&kobj); // no ktype
    
    if (kobject_set_name(&kobj, "ethercat")) {
        EC_ERR("Failed to set module kobject name.\n");
        ret = -ENOMEM;
        goto out_put;
    }
    
    if (kobject_add(&kobj)) {
        EC_ERR("Failed to add module kobject.\n");
        ret = -EEXIST;
        goto out_put;
    }
    
    if (alloc_chrdev_region(&device_number, 0, 1, "EtherCAT")) {
        EC_ERR("Failed to obtain device number!\n");
        ret = -EBUSY;
        goto out_del;
    }

    // zero MAC addresses
    memset(macs, 0x00, sizeof(uint8_t) * MAX_MASTERS * 2 * ETH_ALEN);

    // process MAC parameters
    for (i = 0; i < master_count; i++) {
        if (ec_mac_parse(macs[i][0], main[i], 0)) {
            ret = -EINVAL;
            goto out_cdev;
        }
        
        if (i < backup_count && ec_mac_parse(macs[i][1], backup[i], 1)) {
            ret = -EINVAL;
            goto out_cdev;
        }
    }
    
    if (master_count) {
        if (!(masters = kmalloc(sizeof(ec_master_t) * master_count,
                        GFP_KERNEL))) {
            EC_ERR("Failed to allocate memory for EtherCAT masters.\n");
            ret = -ENOMEM;
            goto out_cdev;
        }
    }
    
    for (i = 0; i < master_count; i++) {
        if (ec_master_init(&masters[i], &kobj, i, macs[i][0], macs[i][1])) {
            ret = -EIO;
            goto out_free_masters;
        }
    }
    
    EC_INFO("%u master%s waiting for devices.\n",
            master_count, (master_count == 1 ? "" : "s"));
    return ret;

out_free_masters:
    for (i--; i >= 0; i--) ec_master_clear(&masters[i]);
    kfree(masters);
out_cdev:
    unregister_chrdev_region(device_number, 1);
out_del:
    kobject_del(&kobj);
out_put:
    kobject_put(&kobj);
    return ret;
}

/*****************************************************************************/

/**
   Module cleanup.
   Clears all master instances.
*/

void __exit ec_cleanup_module(void)
{
    unsigned int i;

    for (i = 0; i < master_count; i++) {
        ec_master_clear(&masters[i]);
    }
    if (master_count)
        kfree(masters);
    
    unregister_chrdev_region(device_number, 1);
    kobject_del(&kobj);
    kobject_put(&kobj);

    EC_INFO("Master module cleaned up.\n");
}

/*****************************************************************************
 * MAC address functions
 ****************************************************************************/

int ec_mac_equal(const uint8_t *mac1, const uint8_t *mac2)
{
    unsigned int i;
    
    for (i = 0; i < ETH_ALEN; i++)
        if (mac1[i] != mac2[i])
            return 0;

    return 1;
}
                
/*****************************************************************************/

ssize_t ec_mac_print(const uint8_t *mac, char *buffer)
{
    off_t off = 0;
    unsigned int i;
    
    for (i = 0; i < ETH_ALEN; i++) {
        off += sprintf(buffer + off, "%02X", mac[i]);
        if (i < ETH_ALEN - 1) off += sprintf(buffer + off, ":");
    }

    return off;
}

/*****************************************************************************/

int ec_mac_is_zero(const uint8_t *mac)
{
    unsigned int i;
    
    for (i = 0; i < ETH_ALEN; i++)
        if (mac[i])
            return 0;

    return 1;
}

/*****************************************************************************/

int ec_mac_is_broadcast(const uint8_t *mac)
{
    unsigned int i;
    
    for (i = 0; i < ETH_ALEN; i++)
        if (mac[i] != 0xff)
            return 0;

    return 1;
}

/*****************************************************************************/

static int ec_mac_parse(uint8_t *mac, const char *src, int allow_empty)
{
    unsigned int i, value;
    const char *orig = src;
    char *rem;

    if (!strlen(src)) {
        if (allow_empty){
            return 0;
        }
        else {
            EC_ERR("MAC address may not be empty.\n");
            return -EINVAL;
        }
    }

    for (i = 0; i < ETH_ALEN; i++) {
        value = simple_strtoul(src, &rem, 16);
        if (rem != src + 2
                || value > 0xFF
                || (i < ETH_ALEN - 1 && *rem != ':')) {
            EC_ERR("Invalid MAC address \"%s\".\n", orig);
            return -EINVAL;
        }
        mac[i] = value;
        if (i < ETH_ALEN - 1)
            src = rem + 1; // skip colon
    }

    return 0;
}

/*****************************************************************************/

/**
   Outputs frame contents for debugging purposes.
*/

void ec_print_data(const uint8_t *data, /**< pointer to data */
                   size_t size /**< number of bytes to output */
                   )
{
    unsigned int i;

    EC_DBG("");
    for (i = 0; i < size; i++) {
        printk("%02X ", data[i]);
        if ((i + 1) % 16 == 0) {
            printk("\n");
            EC_DBG("");
        }
    }
    printk("\n");
}

/*****************************************************************************/

/**
   Outputs frame contents and differences for debugging purposes.
*/

void ec_print_data_diff(const uint8_t *d1, /**< first data */
                        const uint8_t *d2, /**< second data */
                        size_t size /** number of bytes to output */
                        )
{
    unsigned int i;

    EC_DBG("");
    for (i = 0; i < size; i++) {
        if (d1[i] == d2[i]) printk(".. ");
        else printk("%02X ", d2[i]);
        if ((i + 1) % 16 == 0) {
            printk("\n");
            EC_DBG("");
        }
    }
    printk("\n");
}

/*****************************************************************************/

/**
   Prints slave states in clear text.
*/

size_t ec_state_string(uint8_t states, /**< slave states */
                       char *buffer /**< target buffer
                                       (min. EC_STATE_STRING_SIZE bytes) */
                       )
{
    off_t off = 0;
    unsigned int first = 1;

    if (!states) {
        off += sprintf(buffer + off, "(unknown)");
        return off;
    }

    if (states & EC_SLAVE_STATE_INIT) {
        off += sprintf(buffer + off, "INIT");
        first = 0;
    }
    if (states & EC_SLAVE_STATE_PREOP) {
        if (!first) off += sprintf(buffer + off, ", ");
        off += sprintf(buffer + off, "PREOP");
        first = 0;
    }
    if (states & EC_SLAVE_STATE_SAVEOP) {
        if (!first) off += sprintf(buffer + off, ", ");
        off += sprintf(buffer + off, "SAVEOP");
        first = 0;
    }
    if (states & EC_SLAVE_STATE_OP) {
        if (!first) off += sprintf(buffer + off, ", ");
        off += sprintf(buffer + off, "OP");
    }
    if (states & EC_SLAVE_STATE_ACK_ERR) {
        if (!first) off += sprintf(buffer + off, " + ");
        off += sprintf(buffer + off, "ERROR");
    }

    return off;
}

/******************************************************************************
 *  Device interface
 *****************************************************************************/

/**
   Offers an EtherCAT device to a certain master.
   The master decides, if it wants to use the device for EtherCAT operation,
   or not. It is important, that the offered net_device is not used by
   the kernel IP stack. If the master, accepted the offer, the address of
   the newly created EtherCAT device is written to the ecdev pointer, else
   the pointer is written to zero.
   \return 0 on success, else < 0
   \ingroup DeviceInterface
*/

int ecdev_offer(struct net_device *net_dev, /**< net_device to offer */
        ec_pollfunc_t poll, /**< device poll function */
        struct module *module, /**< pointer to the module */
        ec_device_t **ecdev /**< pointer to store a device on success */
        )
{
    ec_master_t *master;
    char str[20];
    unsigned int i;

    for (i = 0; i < master_count; i++) {
        master = &masters[i];

        down(&master->device_sem);
        if (master->main_device.dev) { // master already has a device
            up(&master->device_sem);
            continue;
        }
            
        if (ec_mac_equal(master->main_mac, net_dev->dev_addr)
                || ec_mac_is_broadcast(master->main_mac)) {
            ec_mac_print(net_dev->dev_addr, str);
            EC_INFO("Accepting device %s for master %u.\n",
                    str, master->index);

            ec_device_attach(&master->main_device, net_dev, poll, module);
            up(&master->device_sem);
            
            sprintf(net_dev->name, "ec%u", master->index);
            *ecdev = &master->main_device; // offer accepted
            return 0; // no error
        }
        else {
            up(&master->device_sem);

            if (master->debug_level) {
                ec_mac_print(net_dev->dev_addr, str);
                EC_DBG("Master %u declined device %s.\n", master->index, str);
            }
        }
    }

    *ecdev = NULL; // offer declined
    return 0; // no error
}

/*****************************************************************************/

/**
   Withdraws an EtherCAT device from the master.
   The device is disconnected from the master and all device ressources
   are freed.
   \attention Before calling this function, the ecdev_stop() function has
   to be called, to be sure that the master does not use the device any more.
   \ingroup DeviceInterface
*/

void ecdev_withdraw(ec_device_t *device /**< EtherCAT device */)
{
    ec_master_t *master = device->master;
    char str[20];

    ec_mac_print(device->dev->dev_addr, str);
    EC_INFO("Master %u releasing main device %s.\n", master->index, str);
    
    down(&master->device_sem);
    ec_device_detach(device);
    up(&master->device_sem);
}

/*****************************************************************************/

/**
   Opens the network device and makes the master enter IDLE mode.
   \return 0 on success, else < 0
   \ingroup DeviceInterface
*/

int ecdev_open(ec_device_t *device /**< EtherCAT device */)
{
    if (ec_device_open(device)) {
        EC_ERR("Failed to open device!\n");
        return -1;
    }

    if (ec_master_enter_idle_mode(device->master)) {
        EC_ERR("Failed to enter idle mode!\n");
        return -1;
    }

    return 0;
}

/*****************************************************************************/

/**
   Makes the master leave IDLE mode and closes the network device.
   \return 0 on success, else < 0
   \ingroup DeviceInterface
*/

void ecdev_close(ec_device_t *device /**< EtherCAT device */)
{
    ec_master_leave_idle_mode(device->master);

    if (ec_device_close(device))
        EC_WARN("Failed to close device!\n");
}

/******************************************************************************
 *  Realtime interface
 *****************************************************************************/

/**
 * Returns the version magic of the realtime interface.
 * \return ECRT version magic.
 * \ingroup RealtimeInterface
 */

unsigned int ecrt_version_magic(void)
{
    return ECRT_VERSION_MAGIC;
}

/*****************************************************************************/

/**
   Reserves an EtherCAT master for realtime operation.
   \return pointer to reserved master, or NULL on error
   \ingroup RealtimeInterface
*/

ec_master_t *ecrt_request_master(unsigned int master_index
                                 /**< master index */
                                 )
{
    ec_master_t *master;

    EC_INFO("Requesting master %u...\n", master_index);

    if (master_index >= master_count) {
        EC_ERR("Invalid master index %u.\n", master_index);
        goto out_return;
    }
    master = &masters[master_index];

    down(&master_sem);
    if (master->reserved) {
        up(&master_sem);
        EC_ERR("Master %u is already in use!\n", master_index);
        goto out_return;
    }
    master->reserved = 1;
    up(&master_sem);

    down(&master->device_sem);
    
    if (master->mode != EC_MASTER_MODE_IDLE) {
        up(&master->device_sem);
        EC_ERR("Master %u still waiting for devices!\n", master_index);
        goto out_release;
    }

    if (!try_module_get(master->main_device.module)) {
        up(&master->device_sem);
        EC_ERR("Device module is unloading!\n");
        goto out_release;
    }

    up(&master->device_sem);

    if (!master->main_device.link_state) {
        EC_ERR("Link is DOWN.\n");
        goto out_module_put;
    }

    if (ec_master_enter_operation_mode(master)) {
        EC_ERR("Failed to enter OPERATION mode!\n");
        goto out_module_put;
    }

    EC_INFO("Successfully requested master %u.\n", master_index);
    return master;

 out_module_put:
    module_put(master->main_device.module);
 out_release:
    master->reserved = 0;
 out_return:
    return NULL;
}

/*****************************************************************************/

/**
   Releases a reserved EtherCAT master.
   \ingroup RealtimeInterface
*/

void ecrt_release_master(ec_master_t *master /**< EtherCAT master */)
{
    EC_INFO("Releasing master %u...\n", master->index);

    if (master->mode != EC_MASTER_MODE_OPERATION) {
        EC_WARN("Master %u was was not requested!\n", master->index);
        return;
    }

    ec_master_leave_operation_mode(master);

    module_put(master->main_device.module);
    master->reserved = 0;

    EC_INFO("Released master %u.\n", master->index);
}

/*****************************************************************************/

/** \cond */

module_init(ec_init_module);
module_exit(ec_cleanup_module);

EXPORT_SYMBOL(ecdev_offer);
EXPORT_SYMBOL(ecdev_withdraw);
EXPORT_SYMBOL(ecdev_open);
EXPORT_SYMBOL(ecdev_close);
EXPORT_SYMBOL(ecrt_request_master);
EXPORT_SYMBOL(ecrt_release_master);
EXPORT_SYMBOL(ecrt_version_magic);

/** \endcond */

/*****************************************************************************/
