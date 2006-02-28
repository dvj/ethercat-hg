/******************************************************************************
 *
 *  m o d u l e . c
 *
 *  EtherCAT-Master-Treiber
 *
 *  Autoren: Wilhelm Hagemeister, Florian Pose
 *
 *  $Id$
 *
 *  (C) Copyright IgH 2005
 *  Ingenieurgemeinschaft IgH
 *  Heinz-B�cker Str. 34
 *  D-45356 Essen
 *  Tel.: +49 201/61 99 31
 *  Fax.: +49 201/61 98 36
 *  E-mail: sp@igh-essen.com
 *
 *****************************************************************************/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>

#include "master.h"
#include "device.h"

/*****************************************************************************/

int __init ec_init_module(void);
void __exit ec_cleanup_module(void);

/*****************************************************************************/

#define EC_LIT(X) #X
#define EC_STR(X) EC_LIT(X)

#define COMPILE_INFO "Revision " EC_STR(EC_REV) \
                     ", compiled by " EC_STR(EC_USER) \
                     " at " EC_STR(EC_DATE)

/*****************************************************************************/

int ec_master_count = 1;
ec_master_t *ec_masters = NULL;
int *ec_masters_reserved = NULL;

/*****************************************************************************/

MODULE_AUTHOR ("Florian Pose <fp@igh-essen.com>");
MODULE_DESCRIPTION ("EtherCAT master driver module");
MODULE_LICENSE("GPL");
MODULE_VERSION(COMPILE_INFO);

module_param(ec_master_count, int, 1);
MODULE_PARM_DESC(ec_master_count, "Number of EtherCAT master to initialize.");

/*****************************************************************************/

/**
   Init-Funktion des EtherCAT-Master-Treibermodules

   Initialisiert soviele Master, wie im Parameter ec_master_count
   angegeben wurde (Default ist 1).

   \return 0 wenn alles ok, < 0 bei ung�ltiger Anzahl Master
           oder zu wenig Speicher.
*/

int __init ec_init_module(void)
{
    unsigned int i;

    EC_INFO("Master driver, %s\n", COMPILE_INFO);

    if (ec_master_count < 1) {
        EC_ERR("Error - Illegal ec_master_count: %i\n", ec_master_count);
        return -1;
    }

    EC_INFO("Initializing %i EtherCAT master(s)...\n", ec_master_count);

    if ((ec_masters = (ec_master_t *) kmalloc(sizeof(ec_master_t)
                                              * ec_master_count,
                                              GFP_KERNEL)) == NULL) {
        EC_ERR("Could not allocate memory for EtherCAT master(s)!\n");
        return -1;
    }

    if ((ec_masters_reserved =
         (int *) kmalloc(sizeof(int) * ec_master_count, GFP_KERNEL)) == NULL) {
        EC_ERR("Could not allocate memory for reservation flags!\n");
        kfree(ec_masters);
        return -1;
    }

    for (i = 0; i < ec_master_count; i++) {
        ec_master_init(ec_masters + i);
        ec_masters_reserved[i] = 0;
    }

    EC_INFO("Master driver initialized.\n");

    return 0;
}

/*****************************************************************************/

/**
   Cleanup-Funktion des EtherCAT-Master-Treibermoduls

   Entfernt alle Master-Instanzen.
*/

void __exit ec_cleanup_module(void)
{
    unsigned int i;

    EC_INFO("Cleaning up master driver...\n");

    if (ec_masters) {
        for (i = 0; i < ec_master_count; i++) {
            if (ec_masters_reserved[i]) {
                EC_WARN("Master %i is still in use!\n", i);
            }
            ec_master_clear(&ec_masters[i]);
        }
        kfree(ec_masters);
    }

    EC_INFO("Master driver cleaned up.\n");
}

/******************************************************************************
 *
 * Treiberschnittstelle
 *
 *****************************************************************************/

/**
   Registeriert das EtherCAT-Geraet fuer einen EtherCAT-Master.

   \return 0 wenn alles ok, oder < 0 wenn bereits ein Ger�t registriert
           oder das Geraet nicht ge�ffnet werden konnte.
*/

ec_device_t *EtherCAT_dev_register(unsigned int master_index,
                                   /**< Index des EtherCAT-Masters */
                                   struct net_device *net_dev,
                                   /**< net_device des EtherCAT-Ger�tes */
                                   irqreturn_t (*isr)(int, void *,
                                                      struct pt_regs *),
                                   /**< Interrupt-Service-Routine */
                                   struct module *module
                                   /**< Zeiger auf das Modul */
                                   )
{
    ec_device_t *device;
    ec_master_t *master;

    if (master_index >= ec_master_count) {
        EC_ERR("Master %i does not exist!\n", master_index);
        return NULL;
    }

    if (!net_dev) {
        EC_WARN("Device is NULL!\n");
        return NULL;
    }

    master = ec_masters + master_index;

    if (master->device_registered) {
        EC_ERR("Master %i already has a device!\n", master_index);
        return NULL;
    }

    device = &master->device;

    if (ec_device_init(device, master) < 0) return NULL;

    device->dev = net_dev;
    device->tx_skb->dev = net_dev;
    device->isr = isr;
    device->module = module;

    master->device_registered = 1;

    return device;
}

/*****************************************************************************/

/**
   Hebt die Registrierung eines EtherCAT-Ger�tes auf.
*/

void EtherCAT_dev_unregister(unsigned int master_index,
                             /**< Index des EtherCAT-Masters */
                             ec_device_t *device
                             /**< EtherCAT-Geraet */
                             )
{
    ec_master_t *master;

    if (master_index >= ec_master_count) {
        EC_WARN("Master %i does not exist!\n", master_index);
        return;
    }

    master = ec_masters + master_index;

    if (!master->device_registered || &master->device != device) {
        EC_WARN("Unable to unregister device!\n");
        return;
    }

    master->device_registered = 0;
    ec_device_clear(device);
}

/******************************************************************************
 *
 * Echtzeitschnittstelle
 *
 *****************************************************************************/

/**
   Reserviert einen bestimmten EtherCAT-Master und das zugeh�rige Ger�t.

   Gibt einen Zeiger auf den reservierten EtherCAT-Master zurueck.

   \return Zeiger auf EtherCAT-Master oder NULL, wenn Parameter ungueltig.
*/

ec_master_t *EtherCAT_rt_request_master(unsigned int index
                                        /**< EtherCAT-Master-Index */
                                        )
{
    ec_master_t *master;

    EC_INFO("===== Starting master %i... =====\n", index);

    if (index < 0 || index >= ec_master_count) {
        EC_ERR("Master %i does not exist!\n", index);
        goto req_return;
    }

    if (ec_masters_reserved[index]) {
        EC_ERR("Master %i already in use!\n", index);
        goto req_return;
    }

    master = &ec_masters[index];

    if (!master->device_registered) {
        EC_ERR("Master %i has no device assigned yet!\n", index);
        goto req_return;
    }

    if (!try_module_get(master->device.module)) {
        EC_ERR("Failed to reserve device module!\n");
        goto req_return;
    }

    if (ec_master_open(master) < 0) {
        EC_ERR("Failed to open device!\n");
        goto req_module_put;
    }

    if (ec_scan_for_slaves(master) != 0) {
        EC_ERR("Bus scan failed!\n");
        goto req_close;
    }

    ec_masters_reserved[index] = 1;
    EC_INFO("===== Master %i ready. =====\n", index);

    return master;

 req_close:
    ec_master_close(master);

 req_module_put:
    module_put(master->device.module);
    ec_master_reset(master);

 req_return:
    EC_INFO("===== Failed to start master %i =====\n", index);
    return NULL;
}

/*****************************************************************************/

/**
   Gibt einen zuvor angeforderten EtherCAT-Master wieder frei.
*/

void EtherCAT_rt_release_master(ec_master_t *master /**< EtherCAT-Masdter */)
{
    unsigned int i, found;

    found = 0;
    for (i = 0; i < ec_master_count; i++) {
        if (&ec_masters[i] == master) {
            found = 1;
            break;
        }
    }

    if (!found) {
        EC_WARN("Master %X was never requested!\n", (u32) master);
        return;
    }

    EC_INFO("===== Stopping master %i... =====\n", i);

    ec_master_close(master);
    ec_master_reset(master);

    module_put(master->device.module);
    ec_masters_reserved[i] = 0;

    EC_INFO("===== Master %i stopped. =====\n", i);
    return;
}

/*****************************************************************************/

module_init(ec_init_module);
module_exit(ec_cleanup_module);

EXPORT_SYMBOL(EtherCAT_dev_register);
EXPORT_SYMBOL(EtherCAT_dev_unregister);
EXPORT_SYMBOL(EtherCAT_rt_request_master);
EXPORT_SYMBOL(EtherCAT_rt_release_master);

/*****************************************************************************/

/* Emacs-Konfiguration
;;; Local Variables: ***
;;; c-basic-offset:4 ***
;;; End: ***
*/
