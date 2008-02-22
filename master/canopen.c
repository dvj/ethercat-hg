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
   Canopen-over-EtherCAT functions.
*/

/*****************************************************************************/

#include <linux/module.h>

#include "canopen.h"
#include "master.h"

/*****************************************************************************/

ssize_t ec_show_sdo_attribute(struct kobject *, struct attribute *, char *);
ssize_t ec_show_sdo_entry_attribute(struct kobject *, struct attribute *,
                                    char *);
void ec_sdo_clear(struct kobject *);
void ec_sdo_entry_clear(struct kobject *);

/*****************************************************************************/

/** \cond */

EC_SYSFS_READ_ATTR(info);
EC_SYSFS_READ_ATTR(value);

static struct attribute *sdo_def_attrs[] = {
    &attr_info,
    NULL,
};

static struct sysfs_ops sdo_sysfs_ops = {
    .show = &ec_show_sdo_attribute,
    .store = NULL
};

static struct kobj_type ktype_ec_sdo = {
    .release = ec_sdo_clear,
    .sysfs_ops = &sdo_sysfs_ops,
    .default_attrs = sdo_def_attrs
};

static struct attribute *sdo_entry_def_attrs[] = {
    &attr_info,
    &attr_value,
    NULL,
};

static struct sysfs_ops sdo_entry_sysfs_ops = {
    .show = &ec_show_sdo_entry_attribute,
    .store = NULL
};

static struct kobj_type ktype_ec_sdo_entry = {
    .release = ec_sdo_entry_clear,
    .sysfs_ops = &sdo_entry_sysfs_ops,
    .default_attrs = sdo_entry_def_attrs
};

/** \endcond */

/*****************************************************************************/

/**
   Sdo constructor.
*/

int ec_sdo_init(ec_sdo_t *sdo, /**< Sdo */
                uint16_t index, /**< Sdo index */
                ec_slave_t *slave /**< parent slave */
                )
{
    sdo->slave = slave;
    sdo->index = index;
    sdo->object_code = 0x00;
    sdo->name = NULL;
    sdo->subindices = 0;
    INIT_LIST_HEAD(&sdo->entries);

    // init kobject and add it to the hierarchy
    memset(&sdo->kobj, 0x00, sizeof(struct kobject));
    kobject_init(&sdo->kobj);
    sdo->kobj.ktype = &ktype_ec_sdo;
    sdo->kobj.parent = &slave->sdo_kobj;
    if (kobject_set_name(&sdo->kobj, "%4X", sdo->index)) {
        EC_ERR("Failed to set kobj name.\n");
        kobject_put(&sdo->kobj);
        return -1;
    }
    if (kobject_add(&sdo->kobj)) {
        EC_ERR("Failed to add Sdo kobject.\n");
        kobject_put(&sdo->kobj);
        return -1;
    }

    return 0;
}

/*****************************************************************************/

/**
   Sdo destructor.
   Clears and frees an Sdo object.
*/

void ec_sdo_destroy(ec_sdo_t *sdo /**< Sdo */)
{
    ec_sdo_entry_t *entry, *next;

    // free all entries
    list_for_each_entry_safe(entry, next, &sdo->entries, list) {
        list_del(&entry->list);
        ec_sdo_entry_destroy(entry);
    }

    // destroy self
    kobject_del(&sdo->kobj);
    kobject_put(&sdo->kobj);
}

/*****************************************************************************/

/**
   Clear and free Sdo.
   This method is called by the kobject,
   once there are no more references to it.
*/

void ec_sdo_clear(struct kobject *kobj /**< Sdo's kobject */)
{
    ec_sdo_t *sdo = container_of(kobj, ec_sdo_t, kobj);

    if (sdo->name) kfree(sdo->name);

    kfree(sdo);
}

/*****************************************************************************/

/**
 * Get and Sdo entry from an Sdo via its subindex.
 * \return pointer to Sdo entry, or NULL.
 */

ec_sdo_entry_t *ec_sdo_get_entry(
        ec_sdo_t *sdo, /**< Sdo */
        uint8_t subindex /**< entry subindex */
        )
{
    ec_sdo_entry_t *entry;

    list_for_each_entry(entry, &sdo->entries, list) {
        if (entry->subindex != subindex) continue;
        return entry;
    }

    return NULL;
}

/*****************************************************************************/

/**
 * Print Sdo information to a buffer.
 * /return size of bytes written to buffer.
 */ 

ssize_t ec_sdo_info(ec_sdo_t *sdo, /**< Sdo */
                    char *buffer /**< target buffer */
                    )
{
    off_t off = 0;

    off += sprintf(buffer + off, "Index: 0x%04X\n", sdo->index);
    off += sprintf(buffer + off, "Name: %s\n", sdo->name ? sdo->name : "");
    off += sprintf(buffer + off, "Subindices: %i\n", sdo->subindices);

    return off;
}

/*****************************************************************************/

/**
 * Show an Sdo as Sysfs attribute.
 * /return size of bytes written to buffer.
 */ 

ssize_t ec_show_sdo_attribute(struct kobject *kobj, /**< kobject */
                              struct attribute *attr,
                              char *buffer
                              )
{
    ec_sdo_t *sdo = container_of(kobj, ec_sdo_t, kobj);

    if (attr == &attr_info) {
        return ec_sdo_info(sdo, buffer);
    }

    return 0;
}

/*****************************************************************************/

/**
   Sdo entry constructor.
*/

int ec_sdo_entry_init(ec_sdo_entry_t *entry, /**< Sdo entry */
                      uint8_t subindex, /**< Sdo entry subindex */
                      ec_sdo_t *sdo /**< parent Sdo */
                      )
{
    entry->sdo = sdo;
    entry->subindex = subindex;
    entry->data_type = 0x0000;
    entry->bit_length = 0;
    entry->description = NULL;

    // init kobject and add it to the hierarchy
    memset(&entry->kobj, 0x00, sizeof(struct kobject));
    kobject_init(&entry->kobj);
    entry->kobj.ktype = &ktype_ec_sdo_entry;
    entry->kobj.parent = &sdo->kobj;
    if (kobject_set_name(&entry->kobj, "%i", entry->subindex)) {
        EC_ERR("Failed to set kobj name.\n");
        kobject_put(&entry->kobj);
        return -1;
    }
    if (kobject_add(&entry->kobj)) {
        EC_ERR("Failed to add entry kobject.\n");
        kobject_put(&entry->kobj);
        return -1;
    }

    return 0;
}

/*****************************************************************************/

/**
   Sdo entry destructor.
   Clears and frees an Sdo entry object.
*/

void ec_sdo_entry_destroy(ec_sdo_entry_t *entry /**< Sdo entry */)
{
    // destroy self
    kobject_del(&entry->kobj);
    kobject_put(&entry->kobj);
}

/*****************************************************************************/

/**
   Clear and free Sdo entry.
   This method is called by the kobject,
   once there are no more references to it.
*/

void ec_sdo_entry_clear(struct kobject *kobj /**< Sdo entry's kobject */)
{
    ec_sdo_entry_t *entry = container_of(kobj, ec_sdo_entry_t, kobj);

    if (entry->description) kfree(entry->description);

    kfree(entry);
}

/*****************************************************************************/
 
/**
 * Print Sdo entry information to a buffer.
 * \return number of bytes written.
 */

ssize_t ec_sdo_entry_info(ec_sdo_entry_t *entry, /**< Sdo entry */
                          char *buffer /**< target buffer */
                          )
{
    off_t off = 0;

    off += sprintf(buffer + off, "Subindex: 0x%02X\n", entry->subindex);
    off += sprintf(buffer + off, "Description: %s\n",
                   entry->description ? entry->description : "");
    off += sprintf(buffer + off, "Data type: 0x%04X\n", entry->data_type);
    off += sprintf(buffer + off, "Bit length: %i\n", entry->bit_length);

    return off;
}

/*****************************************************************************/

/**
 * Format entry data based on the CANopen data type and print it to a buffer.
 * \return number of bytes written.
 */

ssize_t ec_sdo_entry_format_data(ec_sdo_entry_t *entry, /**< Sdo entry */
                                 ec_sdo_request_t *request, /**< Sdo request */
                                 char *buffer /**< target buffer */
                                 )
{
    off_t off = 0;
    unsigned int i;

    if (entry->data_type == 0x0002) { // int8
        int8_t value;
        if (entry->bit_length != 8)
            goto not_fit;
        value = EC_READ_S8(request->data);
        off += sprintf(buffer + off, "%i (0x%02X)\n", value, value);
    }
    else if (entry->data_type == 0x0003) { // int16
        int16_t value;
        if (entry->bit_length != 16)
            goto not_fit;
        value = EC_READ_S16(request->data);
        off += sprintf(buffer + off, "%i (0x%04X)\n", value, value);
    }
    else if (entry->data_type == 0x0004) { // int32
        int32_t value;
        if (entry->bit_length != 32)
            goto not_fit;
        value = EC_READ_S16(request->data);
        off += sprintf(buffer + off, "%i (0x%08X)\n", value, value);
    }
    else if (entry->data_type == 0x0005) { // uint8
        uint8_t value;
        if (entry->bit_length != 8)
            goto not_fit;
        value = EC_READ_U8(request->data);
        off += sprintf(buffer + off, "%u (0x%02X)\n", value, value);
    }
    else if (entry->data_type == 0x0006) { // uint16
        uint16_t value;
        if (entry->bit_length != 16)
            goto not_fit;
        value = EC_READ_U16(request->data); 
        off += sprintf(buffer + off, "%u (0x%04X)\n", value, value);
    }
    else if (entry->data_type == 0x0007) { // uint32
        uint32_t value;
        if (entry->bit_length != 32)
            goto not_fit;
        value = EC_READ_U32(request->data);
        off += sprintf(buffer + off, "%i (0x%08X)\n", value, value);
    }
    else if (entry->data_type == 0x0009) { // string
        off += sprintf(buffer + off, "%s\n", request->data);
    }
    else {
        off += sprintf(buffer + off, "Unknown data type %04X. Data:\n",
                entry->data_type);
        goto raw_data;
    }
    return off;

not_fit:
    off += sprintf(buffer + off,
            "Invalid bit length %u for data type 0x%04X. Data:\n",
            entry->bit_length, entry->data_type);
raw_data:
    for (i = 0; i < request->size; i++)
        off += sprintf(buffer + off, "%02X (%c)\n",
                request->data[i], request->data[i]);
    return off;
}

/*****************************************************************************/

/**
 * Start Sdo entry reading.
 * This function blocks, until reading is finished, and is interruptible as
 * long as the master state machine has not begun with reading.
 * \return number of bytes written to buffer, or error code.
 */

ssize_t ec_sdo_entry_read_value(ec_sdo_entry_t *entry, /**< Sdo entry */
                                char *buffer /**< target buffer */
                                )
{
    ec_master_t *master = entry->sdo->slave->master;
    off_t off = 0;
    ec_sdo_request_t request;

    ec_sdo_request_init_read(&request, entry);

    // schedule request.
    down(&master->sdo_sem);
    list_add_tail(&request.list, &master->sdo_requests);
    up(&master->sdo_sem);

    // wait for processing through FSM
    if (wait_event_interruptible(master->sdo_queue,
                request.state != EC_REQUEST_QUEUED)) {
        // interrupted by signal
        down(&master->sdo_sem);
        if (request.state == EC_REQUEST_QUEUED) {
            list_del(&request.list);
            up(&master->sdo_sem);
            return -EINTR;
        }
        // request already processing: interrupt not possible.
        up(&master->sdo_sem);
    }

    // wait until master FSM has finished processing
    wait_event(master->sdo_queue, request.state != EC_REQUEST_IN_PROGRESS);

    if (request.state != EC_REQUEST_COMPLETE)
        return -EIO;

    off += ec_sdo_entry_format_data(entry, &request, buffer);

    ec_sdo_request_clear(&request);
    return off;
}

/*****************************************************************************/

/**
 * Show an Sdo entry as Sysfs attribute.
 * /return size of bytes written to buffer.
 */ 

ssize_t ec_show_sdo_entry_attribute(struct kobject *kobj, /**< kobject */
                                    struct attribute *attr,
                                    char *buffer
                                    )
{
    ec_sdo_entry_t *entry = container_of(kobj, ec_sdo_entry_t, kobj);

    if (attr == &attr_info) {
        return ec_sdo_entry_info(entry, buffer);
    }
    else if (attr == &attr_value) {
        return ec_sdo_entry_read_value(entry, buffer);
    }

    return 0;
}

/*****************************************************************************/

/**
   Sdo request constructor.
*/

void ec_sdo_request_init_read(ec_sdo_request_t *req, /**< Sdo request */
                              ec_sdo_entry_t *entry /**< Sdo entry */
                              )
{
    req->entry = entry;
    req->data = NULL;
    req->size = 0;
    req->state = EC_REQUEST_QUEUED;
}

/*****************************************************************************/

/**
   Sdo request destructor.
*/

void ec_sdo_request_clear(ec_sdo_request_t *req /**< Sdo request */)
{
    if (req->data) kfree(req->data);
}

/*****************************************************************************/
