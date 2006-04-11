/******************************************************************************
 *
 *  m a s t e r . h
 *
 *  Struktur f�r einen EtherCAT-Master.
 *
 *  $Id$
 *
 *****************************************************************************/

#ifndef _EC_MASTER_H_
#define _EC_MASTER_H_

#include <linux/list.h>
#include <linux/sysfs.h>

#include "device.h"
#include "domain.h"

/*****************************************************************************/

/**
   EtherCAT-Rahmen-Statistiken.
*/

typedef struct
{
    unsigned int timeouts; /**< Kommando-Timeouts */
    unsigned int delayed; /**< Verz�gerte Kommandos */
    unsigned int corrupted; /**< Verf�lschte Rahmen */
    unsigned int unmatched; /**< Unpassende Kommandos */
    unsigned int eoe_errors; /**< Ethernet-over-EtherCAT Fehler */
    cycles_t t_last; /**< Timestamp-Counter bei der letzten Ausgabe */
}
ec_stats_t;

/*****************************************************************************/

/**
   EtherCAT-Master

   Verwaltet die EtherCAT-Slaves und kommuniziert mit
   dem zugewiesenen EtherCAT-Ger�t.
*/

struct ec_master
{
    struct list_head list; /**< Noetig fuer Master-Liste */
    struct kobject kobj; /**< Kernel-Object */
    unsigned int index; /**< Master-Index */
    struct list_head slaves; /**< Liste der Slaves auf dem Bus */
    unsigned int slave_count; /**< Anzahl Slaves auf dem Bus */
    ec_device_t *device; /**< EtherCAT-Ger�t */
    struct list_head command_queue; /**< Kommando-Warteschlange */
    uint8_t command_index; /**< Aktueller Kommando-Index */
    struct list_head domains; /**< Liste der Prozessdatendom�nen */
    ec_command_t simple_command; /**< Kommando f�r Initialisierungsphase */
    ec_command_t watch_command; /**< Kommando zum �berwachen der Slaves */
    unsigned int slaves_responding; /**< Anzahl antwortender Slaves */
    ec_slave_state_t slave_states; /**< Zust�nde der antwortenden Slaves */
    int debug_level; /**< Debug-Level im Master-Code */
    ec_stats_t stats; /**< Rahmen-Statistiken */
    unsigned int timeout; /**< Timeout f�r synchronen Datenaustausch */
    struct list_head eoe_slaves; /**< Ethernet over EtherCAT Slaves */
    unsigned int reserved; /**< Master durch Echtzeitprozess reserviert */
};

/*****************************************************************************/

// Master creation and deletion
int ec_master_init(ec_master_t *, unsigned int);
void ec_master_clear(struct kobject *);
void ec_master_reset(ec_master_t *);

// IO
void ec_master_receive(ec_master_t *, const uint8_t *, size_t);
void ec_master_queue_command(ec_master_t *, ec_command_t *);
int ec_master_simple_io(ec_master_t *, ec_command_t *);

// Slave management
int ec_master_bus_scan(ec_master_t *);

// Misc
void ec_master_debug(const ec_master_t *);
void ec_master_output_stats(ec_master_t *);
void ec_master_run_eoe(ec_master_t *);

/*****************************************************************************/

#endif

/* Emacs-Konfiguration
;;; Local Variables: ***
;;; c-basic-offset:4 ***
;;; End: ***
*/
