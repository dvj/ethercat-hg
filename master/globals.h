/******************************************************************************
 *
 *  g l o b a l s . h
 *
 *  Global definitions and macros.
 *
 *  $Id$
 *
 *****************************************************************************/

#ifndef _EC_GLOBALS_
#define _EC_GLOBALS_

#include <linux/types.h>

/*****************************************************************************/

// EtherCAT-Protokoll
#define EC_MAX_FRAME_SIZE 1500 /**< maximum size of an EtherCAT frame (without
                                header and CRC) */
#define EC_MIN_FRAME_SIZE 46 /** ... minimum size */
#define EC_FRAME_HEADER_SIZE 2 /**< size of an EtherCAT frame header */
#define EC_COMMAND_HEADER_SIZE 10 /**< size of an EtherCAT command header */
#define EC_COMMAND_FOOTER_SIZE 2 /**< size of an EtherCAT command footer */
#define EC_SYNC_SIZE 8 /**< size of a sync manager configuration page */
#define EC_FMMU_SIZE 16 /**< size of an FMMU configuration page */
#define EC_MAX_FMMUS 16 /**< maximum number of FMMUs per slave */
#define EC_MAX_DATA_SIZE (EC_MAX_FRAME_SIZE \
                          - EC_FRAME_HEADER_SIZE \
                          - EC_COMMAND_HEADER_SIZE \
                          - EC_COMMAND_FOOTER_SIZE) /**< maximum data size of a
                                                       single command */

/*****************************************************************************/

#define EC_INFO(fmt, args...) \
    printk(KERN_INFO "EtherCAT: " fmt, ##args)
#define EC_ERR(fmt, args...) \
    printk(KERN_ERR "EtherCAT ERROR: " fmt, ##args)
#define EC_WARN(fmt, args...) \
    printk(KERN_WARNING "EtherCAT WARNING: " fmt, ##args)
#define EC_DBG(fmt, args...) \
    printk(KERN_DEBUG "EtherCAT DEBUG: " fmt, ##args)

#define EC_LIT(X) #X
#define EC_STR(X) EC_LIT(X)

#define EC_SYSFS_READ_ATTR(NAME) \
    static struct attribute attr_##NAME = { \
        .name = EC_STR(NAME), .owner = THIS_MODULE, .mode = S_IRUGO \
    }

/*****************************************************************************/

extern void ec_print_data(const uint8_t *, size_t);
extern void ec_print_data_diff(const uint8_t *, const uint8_t *, size_t);

/*****************************************************************************/

/**
   Code - Message pair.
   Some EtherCAT commands support reading a status code to display a certain
   message. This type allows to map a code to a message string.
*/

typedef struct
{
    uint32_t code; /**< code */
    const char *message; /**< message belonging to \a code */
}
ec_code_msg_t;

/*****************************************************************************/

#endif

