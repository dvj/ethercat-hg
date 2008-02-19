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
   EtherCAT process data object methods.
*/

/*****************************************************************************/

#include <linux/slab.h>

#include "pdo.h"

/*****************************************************************************/

void ec_pdo_clear_entries(ec_pdo_t *);

/*****************************************************************************/

/** PDO constructor.
 */
void ec_pdo_init(
        ec_pdo_t *pdo /**< EtherCAT PDO */
        )
{
    pdo->sync_index = -1; // not assigned 
    pdo->name = NULL;
    INIT_LIST_HEAD(&pdo->entries);
}

/*****************************************************************************/

/** Pdo copy constructor.
 */
int ec_pdo_init_copy(ec_pdo_t *pdo, const ec_pdo_t *other_pdo)
{
    pdo->dir = other_pdo->dir;
    pdo->index = other_pdo->index;
    pdo->sync_index = other_pdo->sync_index;
    pdo->name = NULL;
    INIT_LIST_HEAD(&pdo->entries);

    if (ec_pdo_set_name(pdo, other_pdo->name))
        goto out_return;

    if (ec_pdo_copy_entries(pdo, other_pdo))
        goto out_clear;

    return 0;

out_clear:
    ec_pdo_clear(pdo);
out_return:
    return -1;
}

/*****************************************************************************/

/** PDO destructor.
 */
void ec_pdo_clear(ec_pdo_t *pdo /**< EtherCAT PDO */)
{
    if (pdo->name)
        kfree(pdo->name);

    ec_pdo_clear_entries(pdo);
}

/*****************************************************************************/

/** Clear Pdo entry list.
 */
void ec_pdo_clear_entries(ec_pdo_t *pdo /**< EtherCAT PDO */)
{
    ec_pdo_entry_t *entry, *next;

    // free all PDO entries
    list_for_each_entry_safe(entry, next, &pdo->entries, list) {
        list_del(&entry->list);
        ec_pdo_entry_clear(entry);
        kfree(entry);
    }
}

/*****************************************************************************/

/** Set Pdo name.
 */
int ec_pdo_set_name(
        ec_pdo_t *pdo, /**< Pdo. */
        const char *name /**< New name. */
        )
{
    unsigned int len;

    if (pdo->name)
        kfree(pdo->name);

    if (name && (len = strlen(name))) {
        if (!(pdo->name = (char *) kmalloc(len + 1, GFP_KERNEL))) {
            EC_ERR("Failed to allocate PDO name.\n");
            return -1;
        }
        memcpy(pdo->name, name, len + 1);
    } else {
        pdo->name = NULL;
    }

    return 0;
}

/*****************************************************************************/

/** Copy Pdo entries from another Pdo.
 */
int ec_pdo_copy_entries(ec_pdo_t *pdo, const ec_pdo_t *other)
{
    ec_pdo_entry_t *entry, *other_entry;

    ec_pdo_clear_entries(pdo);

    list_for_each_entry(other_entry, &other->entries, list) {
        if (!(entry = (ec_pdo_entry_t *)
                    kmalloc(sizeof(ec_pdo_entry_t), GFP_KERNEL))) {
            EC_ERR("Failed to allocate memory for PDO entry copy.\n");
            return -1;
        }

        if (ec_pdo_entry_init_copy(entry, other_entry)) {
            kfree(entry);
            return -1;
        }

        list_add_tail(&entry->list, &pdo->entries);
    }

    return 0;
}

/*****************************************************************************/

/** Pdo entry constructor.
 */
void ec_pdo_entry_init(
        ec_pdo_entry_t *entry /**< Pdo entry. */
        )
{
    entry->name = NULL;
}

/*****************************************************************************/

/** Pdo entry copy constructor.
 */
int ec_pdo_entry_init_copy(
        ec_pdo_entry_t *entry, /**< Pdo entry. */
        const ec_pdo_entry_t *other /**< Pdo entry to copy from. */
        )
{
    entry->index = other->index;
    entry->subindex = other->subindex;
    entry->name = NULL;
    entry->bit_length = other->bit_length;

    if (ec_pdo_entry_set_name(entry, other->name))
        return -1;

    return 0;
}

/*****************************************************************************/

/** Pdo entry destructor.
 */
void ec_pdo_entry_clear(ec_pdo_entry_t *entry /**< Pdo entry. */)
{
    if (entry->name)
        kfree(entry->name);
}

/*****************************************************************************/

/** Set Pdo entry name.
 */
int ec_pdo_entry_set_name(
        ec_pdo_entry_t *entry, /**< Pdo entry. */
        const char *name /**< New name. */
        )
{
    unsigned int len;

    if (entry->name)
        kfree(entry->name);

    if (name && (len = strlen(name))) {
        if (!(entry->name = (char *) kmalloc(len + 1, GFP_KERNEL))) {
            EC_ERR("Failed to allocate PDO entry name.\n");
            return -1;
        }
        memcpy(entry->name, name, len + 1);
    } else {
        entry->name = NULL;
    }

    return 0;
}

/*****************************************************************************/
