/******************************************************************************
 *
 *  c o m m a n d . c
 *
 *  Methods of an EtherCAT command.
 *
 *  $Id$
 *
 *****************************************************************************/

#include <linux/slab.h>
#include <linux/delay.h>

#include "command.h"
#include "master.h"

/*****************************************************************************/

#define EC_FUNC_HEADER \
    if (unlikely(ec_command_prealloc(command, data_size))) \
        return -1; \
    command->index = 0; \
    command->working_counter = 0; \
    command->state = EC_CMD_INIT;

#define EC_FUNC_FOOTER \
    command->data_size = data_size; \
    memset(command->data, 0x00, data_size); \
    return 0;

/*****************************************************************************/

/**
   Command constructor.
*/

void ec_command_init(ec_command_t *command)
{
    command->type = EC_CMD_NONE;
    command->address.logical = 0x00000000;
    command->data = NULL;
    command->mem_size = 0;
    command->data_size = 0;
    command->index = 0x00;
    command->working_counter = 0x00;
    command->state = EC_CMD_INIT;
}

/*****************************************************************************/

/**
   Command destructor.
*/

void ec_command_clear(ec_command_t *command)
{
    if (command->data) kfree(command->data);
}

/*****************************************************************************/

/**
   Allocates command data memory.
   \return 0 in case of success, else < 0
*/

int ec_command_prealloc(ec_command_t *command, size_t size)
{
    if (size <= command->mem_size) return 0;

    if (command->data) {
        kfree(command->data);
        command->data = NULL;
        command->mem_size = 0;
    }

    if (!(command->data = kmalloc(size, GFP_KERNEL))) {
        EC_ERR("Failed to allocate %i bytes of command memory!\n", size);
        return -1;
    }

    command->mem_size = size;
    return 0;
}

/*****************************************************************************/

/**
   Initializes an EtherCAT NPRD command.
   Node-adressed physical read.
   \return 0 in case of success, else < 0
*/

int ec_command_nprd(ec_command_t *command,
                    /**< EtherCAT command */
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
    command->type = EC_CMD_NPRD;
    command->address.physical.slave = node_address;
    command->address.physical.mem = offset;
    EC_FUNC_FOOTER;
}

/*****************************************************************************/

/**
   Initializes an EtherCAT NPWR command.
   Node-adressed physical write.
   \return 0 in case of success, else < 0
*/

int ec_command_npwr(ec_command_t *command,
                    /**< EtherCAT command */
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
    command->type = EC_CMD_NPWR;
    command->address.physical.slave = node_address;
    command->address.physical.mem = offset;
    EC_FUNC_FOOTER;
}

/*****************************************************************************/

/**
   Initializes an EtherCAT APRD command.
   Autoincrement physical read.
   \return 0 in case of success, else < 0
*/

int ec_command_aprd(ec_command_t *command,
                    /**< EtherCAT command */
                    uint16_t ring_position,
                    /**< auto-increment position */
                    uint16_t offset,
                    /**< physical memory address */
                    size_t data_size
                    /**< number of bytes to read */
                    )
{
    EC_FUNC_HEADER;
    command->type = EC_CMD_APRD;
    command->address.physical.slave = (int16_t) ring_position * (-1);
    command->address.physical.mem = offset;
    EC_FUNC_FOOTER;
}

/*****************************************************************************/

/**
   Initializes an EtherCAT APWR command.
   Autoincrement physical write.
   \return 0 in case of success, else < 0
*/

int ec_command_apwr(ec_command_t *command,
                    /**< EtherCAT command */
                    uint16_t ring_position,
                    /**< auto-increment position */
                    uint16_t offset,
                    /**< physical memory address */
                    size_t data_size
                    /**< number of bytes to write */
                    )
{
    EC_FUNC_HEADER;
    command->type = EC_CMD_APWR;
    command->address.physical.slave = (int16_t) ring_position * (-1);
    command->address.physical.mem = offset;
    EC_FUNC_FOOTER;
}

/*****************************************************************************/

/**
   Initializes an EtherCAT BRD command.
   Broadcast read.
   \return 0 in case of success, else < 0
*/

int ec_command_brd(ec_command_t *command,
                   /**< EtherCAT command */
                   uint16_t offset,
                   /**< physical memory address */
                   size_t data_size
                   /**< number of bytes to read */
                   )
{
    EC_FUNC_HEADER;
    command->type = EC_CMD_BRD;
    command->address.physical.slave = 0x0000;
    command->address.physical.mem = offset;
    EC_FUNC_FOOTER;
}

/*****************************************************************************/

/**
   Initializes an EtherCAT BWR command.
   Broadcast write.
   \return 0 in case of success, else < 0
*/

int ec_command_bwr(ec_command_t *command,
                   /**< EtherCAT command */
                   uint16_t offset,
                   /**< physical memory address */
                   size_t data_size
                   /**< number of bytes to write */
                   )
{
    EC_FUNC_HEADER;
    command->type = EC_CMD_BWR;
    command->address.physical.slave = 0x0000;
    command->address.physical.mem = offset;
    EC_FUNC_FOOTER;
}

/*****************************************************************************/

/**
   Initializes an EtherCAT LRW command.
   Logical read write.
   \return 0 in case of success, else < 0
*/

int ec_command_lrw(ec_command_t *command,
                   /**< EtherCAT command */
                   uint32_t offset,
                   /**< logical address */
                   size_t data_size
                   /**< number of bytes to read/write */
                   )
{
    EC_FUNC_HEADER;
    command->type = EC_CMD_LRW;
    command->address.logical = offset;
    EC_FUNC_FOOTER;
}

/*****************************************************************************/
