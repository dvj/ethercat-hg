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
   Methods of an EtherCAT datagram.
*/

/*****************************************************************************/

#include <linux/slab.h>

#include "datagram.h"
#include "master.h"

/*****************************************************************************/

/** \cond */

#define EC_FUNC_HEADER \
    if (unlikely(ec_datagram_prealloc(datagram, data_size))) \
        return -1; \
    datagram->index = 0; \
    datagram->working_counter = 0; \
    datagram->state = EC_DATAGRAM_INIT;

#define EC_FUNC_FOOTER \
    datagram->data_size = data_size; \
    memset(datagram->data, 0x00, data_size); \
    return 0;

/** \endcond */

/*****************************************************************************/

/**
   Datagram constructor.
*/

void ec_datagram_init(ec_datagram_t *datagram /**< EtherCAT datagram */)
{
    INIT_LIST_HEAD(&datagram->queue); // mark as unqueued
    datagram->type = EC_DATAGRAM_NONE;
    memset(datagram->address, 0x00, EC_ADDR_LEN);
    datagram->data = NULL;
    datagram->mem_size = 0;
    datagram->data_size = 0;
    datagram->index = 0x00;
    datagram->working_counter = 0x00;
    datagram->state = EC_DATAGRAM_INIT;
    datagram->cycles_sent = 0;
    datagram->jiffies_sent = 0;
    datagram->cycles_received = 0;
    datagram->jiffies_received = 0;
    datagram->skip_count = 0;
    datagram->stats_output_jiffies = 0;
    datagram->name[0] = 0x00;
}

/*****************************************************************************/

/**
   Datagram destructor.
*/

void ec_datagram_clear(ec_datagram_t *datagram /**< EtherCAT datagram */)
{
    if (datagram->data) kfree(datagram->data);
}

/*****************************************************************************/

/**
   Allocates datagram data memory.
   If the allocated memory is already larger than requested, nothing ist done.
   \return 0 in case of success, else < 0
*/

int ec_datagram_prealloc(ec_datagram_t *datagram, /**< EtherCAT datagram */
                         size_t size /**< New size in bytes */
                         )
{
    if (size <= datagram->mem_size) return 0;

    if (datagram->data) {
        kfree(datagram->data);
        datagram->data = NULL;
        datagram->mem_size = 0;
    }

    if (!(datagram->data = kmalloc(size, GFP_ATOMIC))) {
        EC_ERR("Failed to allocate %i bytes of datagram memory!\n", size);
        return -1;
    }

    datagram->mem_size = size;
    return 0;
}

/*****************************************************************************/

/**
   Initializes an EtherCAT NPRD datagram.
   Node-adressed physical read.
   \return 0 in case of success, else < 0
*/

int ec_datagram_nprd(ec_datagram_t *datagram,
                     /**< EtherCAT datagram */
                     uint16_t node_address,
                     /**< configured station address */
                     uint16_t offset,
                     /**< physical memory address */
                     size_t data_size
                     /**< number of bytes to read */
                     )
{
    if (unlikely(node_address == 0x0000))
        EC_WARN("Using node address 0x0000!\n");

    EC_FUNC_HEADER;
    datagram->type = EC_DATAGRAM_NPRD;
    EC_WRITE_U16(datagram->address, node_address);
    EC_WRITE_U16(datagram->address + 2, offset);
    EC_FUNC_FOOTER;
}

/*****************************************************************************/

/**
   Initializes an EtherCAT NPWR datagram.
   Node-adressed physical write.
   \return 0 in case of success, else < 0
*/

int ec_datagram_npwr(ec_datagram_t *datagram,
                     /**< EtherCAT datagram */
                     uint16_t node_address,
                     /**< configured station address */
                     uint16_t offset,
                     /**< physical memory address */
                     size_t data_size
                     /**< number of bytes to write */
                     )
{
    if (unlikely(node_address == 0x0000))
        EC_WARN("Using node address 0x0000!\n");

    EC_FUNC_HEADER;
    datagram->type = EC_DATAGRAM_NPWR;
    EC_WRITE_U16(datagram->address, node_address);
    EC_WRITE_U16(datagram->address + 2, offset);
    EC_FUNC_FOOTER;
}

/*****************************************************************************/

/**
   Initializes an EtherCAT APRD datagram.
   Autoincrement physical read.
   \return 0 in case of success, else < 0
*/

int ec_datagram_aprd(ec_datagram_t *datagram,
                     /**< EtherCAT datagram */
                     uint16_t ring_position,
                     /**< auto-increment position */
                     uint16_t offset,
                     /**< physical memory address */
                     size_t data_size
                     /**< number of bytes to read */
                     )
{
    EC_FUNC_HEADER;
    datagram->type = EC_DATAGRAM_APRD;
    EC_WRITE_S16(datagram->address, (int16_t) ring_position * (-1));
    EC_WRITE_U16(datagram->address + 2, offset);
    EC_FUNC_FOOTER;
}

/*****************************************************************************/

/**
   Initializes an EtherCAT APWR datagram.
   Autoincrement physical write.
   \return 0 in case of success, else < 0
*/

int ec_datagram_apwr(ec_datagram_t *datagram,
                     /**< EtherCAT datagram */
                     uint16_t ring_position,
                     /**< auto-increment position */
                     uint16_t offset,
                     /**< physical memory address */
                     size_t data_size
                     /**< number of bytes to write */
                     )
{
    EC_FUNC_HEADER;
    datagram->type = EC_DATAGRAM_APWR;
    EC_WRITE_S16(datagram->address, (int16_t) ring_position * (-1));
    EC_WRITE_U16(datagram->address + 2, offset);
    EC_FUNC_FOOTER;
}

/*****************************************************************************/

/**
   Initializes an EtherCAT BRD datagram.
   Broadcast read.
   \return 0 in case of success, else < 0
*/

int ec_datagram_brd(ec_datagram_t *datagram,
                    /**< EtherCAT datagram */
                    uint16_t offset,
                    /**< physical memory address */
                    size_t data_size
                    /**< number of bytes to read */
                    )
{
    EC_FUNC_HEADER;
    datagram->type = EC_DATAGRAM_BRD;
    EC_WRITE_U16(datagram->address, 0x0000);
    EC_WRITE_U16(datagram->address + 2, offset);
    EC_FUNC_FOOTER;
}

/*****************************************************************************/

/**
   Initializes an EtherCAT BWR datagram.
   Broadcast write.
   \return 0 in case of success, else < 0
*/

int ec_datagram_bwr(ec_datagram_t *datagram,
                    /**< EtherCAT datagram */
                    uint16_t offset,
                    /**< physical memory address */
                    size_t data_size
                    /**< number of bytes to write */
                    )
{
    EC_FUNC_HEADER;
    datagram->type = EC_DATAGRAM_BWR;
    EC_WRITE_U16(datagram->address, 0x0000);
    EC_WRITE_U16(datagram->address + 2, offset);
    EC_FUNC_FOOTER;
}

/*****************************************************************************/

/**
   Initializes an EtherCAT LRW datagram.
   Logical read write.
   \return 0 in case of success, else < 0
*/

int ec_datagram_lrw(ec_datagram_t *datagram,
                    /**< EtherCAT datagram */
                    uint32_t offset,
                    /**< logical address */
                    size_t data_size
                    /**< number of bytes to read/write */
                    )
{
    EC_FUNC_HEADER;
    datagram->type = EC_DATAGRAM_LRW;
    EC_WRITE_U32(datagram->address, offset);
    EC_FUNC_FOOTER;
}

/*****************************************************************************/

/**
 * Evaluates the working counter of a single-cast datagram.
 * Outputs an error message.
 */

void ec_datagram_print_wc_error(
        const ec_datagram_t *datagram /**< EtherCAT datagram */
        )
{
    if (datagram->working_counter == 0)
        printk("No response.");
    else if (datagram->working_counter > 1)
        printk("%u slaves responded!", datagram->working_counter);
    else
        printk("Success.");
    printk("\n");
}

/*****************************************************************************/

/**
 * Outputs datagram statistics at most every second.
 */

void ec_datagram_output_stats(
        ec_datagram_t *datagram
        )
{
    if (jiffies - datagram->stats_output_jiffies < HZ) {
        datagram->stats_output_jiffies = jiffies;
    
        if (unlikely(datagram->skip_count)) {
            EC_WARN("Datagram %x (%s) was SKIPPED %u time%s.\n",
                    (unsigned int) datagram, datagram->name,
                    datagram->skip_count,
                    datagram->skip_count == 1 ? "" : "s");
            datagram->skip_count = 0;
        }
    }
}

/*****************************************************************************/
