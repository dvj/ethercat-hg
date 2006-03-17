/******************************************************************************
 *
 * Oeffentliche EtherCAT-Schnittstellen fuer EtherCAT-Geraetetreiber.
 *
 * $Id$
 *
 *****************************************************************************/

#ifndef _ETHERCAT_DEVICE_H_
#define _ETHERCAT_DEVICE_H_

#include <linux/netdevice.h>

/*****************************************************************************/

struct ec_device;
typedef struct ec_device ec_device_t;

typedef irqreturn_t (*ec_isr_t)(int, void *, struct pt_regs *);

/*****************************************************************************/
// Registration functions

ec_device_t *ecdev_register(unsigned int master_index,
                            struct net_device *net_dev, ec_isr_t isr,
                            struct module *module);
void ecdev_unregister(unsigned int master_index, ec_device_t *device);

/*****************************************************************************/
// Device methods

int ecdev_is_ec(const ec_device_t *device, const struct net_device *net_dev);
void ecdev_receive(ec_device_t *device, const void *data, size_t size);
void ecdev_link_state(ec_device_t *device, uint8_t newstate);

/*****************************************************************************/

#endif
