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

#include <linux/module.h>
#include <linux/timer.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>

#include "../../include/ecrt.h" // EtherCAT realtime interface
#include "../../include/ecdb.h" // EtherCAT slave database

#define FREQUENCY 100

//#define KBUS

/*****************************************************************************/

struct timer_list timer;

// EtherCAT
ec_master_t *master = NULL;
ec_domain_t *domain1 = NULL;
spinlock_t master_lock = SPIN_LOCK_UNLOCKED;

// data fields
#ifdef KBUS
void *r_inputs;
void *r_outputs;
#endif

void *r_dig_out;

#if 1
ec_pdo_reg_t domain1_pdos[] = {
    {"2", Beckhoff_EL2004_Outputs, &r_dig_out},
    {}
};
#endif

/*****************************************************************************/

void run(unsigned long data)
{
    static unsigned int counter = 0;
    static unsigned int blink = 0;

    // receive
    spin_lock(&master_lock);
    ecrt_master_receive(master);
    ecrt_domain_process(domain1);
    spin_unlock(&master_lock);

    // process data
    // k_pos = EC_READ_U32(r_ssi);
    EC_WRITE_U8(r_dig_out, blink ? 0x0F : 0x00);

    if (counter) {
        counter--;
    }
    else {
        counter = FREQUENCY;
        blink = !blink;
    }

#ifdef KBUS
    EC_WRITE_U8(r_outputs + 2, blink ? 0xFF : 0x00);
#endif

    // send
    spin_lock(&master_lock);
    ecrt_domain_queue(domain1);
    spin_unlock(&master_lock);

    ecrt_master_run(master);

    spin_lock(&master_lock);
    ecrt_master_send(master);
    spin_unlock(&master_lock);

    // restart timer
    timer.expires += HZ / FREQUENCY;
    add_timer(&timer);
}

/*****************************************************************************/

int request_lock(void *data)
{
    spin_lock(&master_lock);
    return 0; // access allowed
}

/*****************************************************************************/

void release_lock(void *data)
{
    spin_unlock(&master_lock);
}

/*****************************************************************************/

int __init init_mini_module(void)
{
#if 0
    ec_slave_t *slave;
#endif

    printk(KERN_INFO "=== Starting Minimal EtherCAT environment... ===\n");

    if (!(master = ecrt_request_master(0))) {
        printk(KERN_ERR "Requesting master 0 failed!\n");
        goto out_return;
    }

    ecrt_master_callbacks(master, request_lock, release_lock, NULL);

    printk(KERN_INFO "Registering domain...\n");
    if (!(domain1 = ecrt_master_create_domain(master))) {
        printk(KERN_ERR "Domain creation failed!\n");
        goto out_release_master;
    }

    printk(KERN_INFO "Registering PDOs...\n");
#if 1
    if (ecrt_domain_register_pdo_list(domain1, domain1_pdos)) {
        printk(KERN_ERR "PDO registration failed!\n");
        goto out_release_master;
    }
#endif

#ifdef KBUS
    if (!ecrt_domain_register_pdo_range(domain1, "0", Beckhoff_BK1120,
                                        EC_DIR_OUTPUT, 0, 4, &r_outputs)) {
        printk(KERN_ERR "PDO registration failed!\n");
        goto out_release_master;
    }
    if (!ecrt_domain_register_pdo_range(domain1, "0", Beckhoff_BK1120,
                                        EC_DIR_INPUT, 0, 4, &r_inputs)) {
        printk(KERN_ERR "PDO registration failed!\n");
        goto out_release_master;
    }
#endif

#if 0
    if (!(slave = ecrt_master_get_slave(master, "2")))
        goto out_release_master;

    if (ecrt_slave_conf_sdo8(slave, 0x4061, 1, 0))
        goto out_release_master;
#endif

    printk(KERN_INFO "Activating master...\n");
    if (ecrt_master_activate(master)) {
        printk(KERN_ERR "Failed to activate master!\n");
        goto out_release_master;
    }

    printk("Starting cyclic sample thread.\n");
    init_timer(&timer);
    timer.function = run;
    timer.expires = jiffies + 10;
    add_timer(&timer);

    printk(KERN_INFO "=== Minimal EtherCAT environment started. ===\n");
    return 0;

 out_release_master:
    ecrt_release_master(master);
 out_return:
    return -1;
}

/*****************************************************************************/

void __exit cleanup_mini_module(void)
{
    printk(KERN_INFO "=== Stopping Minimal EtherCAT environment... ===\n");

    del_timer_sync(&timer);
    printk(KERN_INFO "Releasing master...\n");
    ecrt_release_master(master);

    printk(KERN_INFO "=== Minimal EtherCAT environment stopped. ===\n");
}

/*****************************************************************************/

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Florian Pose <fp@igh-essen.com>");
MODULE_DESCRIPTION("EtherCAT minimal test environment");

module_init(init_mini_module);
module_exit(cleanup_mini_module);

/*****************************************************************************/
