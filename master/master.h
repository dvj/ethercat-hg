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

#include "device.h"
#include "slave.h"
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
    ec_slave_t *slaves; /**< Array von Slaves auf dem Bus */
    unsigned int slave_count; /**< Anzahl Slaves auf dem Bus */
    ec_device_t *device; /**< EtherCAT-Ger�t */
    struct list_head commands; /**< Kommando-Liste */
    uint8_t command_index; /**< Aktueller Kommando-Index */
    struct list_head domains; /**< Liste der Prozessdatendom�nen */
    ec_command_t watch_command; /**< Kommando zum �berwachen der Slaves */
    unsigned int slaves_responding; /**< Anzahl antwortender Slaves */
    ec_slave_state_t slave_states; /**< Zust�nde der antwortenden Slaves */
    int debug_level; /**< Debug-Level im Master-Code */
    ec_stats_t stats; /**< Rahmen-Statistiken */
    unsigned int timeout; /**< Timeout f�r synchronen Datenaustausch */
};

/*****************************************************************************/

// Master creation and deletion
void ec_master_init(ec_master_t *);
void ec_master_clear(ec_master_t *);
void ec_master_reset(ec_master_t *);

// IO
void ec_master_receive(ec_master_t *, const uint8_t *, size_t);
void ec_master_queue_command(ec_master_t *, ec_command_t *);
int ec_master_simple_io(ec_master_t *, ec_command_t *);

// Registration of devices
int ec_master_open(ec_master_t *);
void ec_master_close(ec_master_t *);

// Slave management
int ec_master_bus_scan(ec_master_t *);
ec_slave_t *ec_master_slave_address(const ec_master_t *, const char *);

// Misc
void ec_master_debug(const ec_master_t *);
void ec_master_output_stats(ec_master_t *);

/*****************************************************************************/

#endif

/* Emacs-Konfiguration
;;; Local Variables: ***
;;; c-basic-offset:4 ***
;;; End: ***
*/
