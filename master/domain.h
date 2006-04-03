/******************************************************************************
 *
 *  d o m a i n . h
 *
 *  Struktur f�r eine Gruppe von EtherCAT-Slaves.
 *
 *  $Id$
 *
 *****************************************************************************/

#ifndef _EC_DOMAIN_H_
#define _EC_DOMAIN_H_

#include <linux/list.h>

#include "globals.h"
#include "slave.h"
#include "command.h"

/*****************************************************************************/

/**
   Datenfeld-Konfiguration.
*/

typedef struct
{
    struct list_head list;
    ec_slave_t *slave;
    const ec_sync_t *sync;
    uint32_t field_offset;
    void **data_ptr;
}
ec_field_reg_t;

/*****************************************************************************/

/**
   EtherCAT-Dom�ne

   Verwaltet die Prozessdaten und das hierf�r n�tige Kommando einer bestimmten
   Menge von Slaves.
*/

struct ec_domain
{
    struct list_head list; /**< Listenkopf */
    ec_master_t *master; /**< EtherCAT-Master, zu der die Dom�ne geh�rt. */
    size_t data_size; /**< Gr��e der Prozessdaten */
    struct list_head commands; /**< EtherCAT-Kommandos f�r die Prozessdaten */
    uint32_t base_address; /**< Logische Basisaddresse der Domain */
    unsigned int response_count; /**< Anzahl antwortender Slaves */
    struct list_head field_regs; /**< Liste der Datenfeldregistrierungen */
};

/*****************************************************************************/

void ec_domain_init(ec_domain_t *, ec_master_t *);
void ec_domain_clear(ec_domain_t *);
int ec_domain_alloc(ec_domain_t *, uint32_t);

/*****************************************************************************/

#endif

/* Emacs-Konfiguration
   ;;; Local Variables: ***
   ;;; c-basic-offset:4 ***
;;; End: ***
*/
