/******************************************************************************
 *
 *  s l a v e . h
 *
 *  Struktur f�r einen EtherCAT-Slave.
 *
 *  $Id$
 *
 *****************************************************************************/

#ifndef _EC_SLAVE_H_
#define _EC_SLAVE_H_

#include "types.h"

/*****************************************************************************/

/**
   FMMU-Konfiguration.
*/

typedef struct
{
    const ec_domain_t *domain;
    const ec_sync_t *sync;
    uint32_t logical_start_address;
}
ec_fmmu_t;

/*****************************************************************************/

/**
   EtherCAT-Slave
*/

struct ec_slave
{
    ec_master_t *master; /**< EtherCAT-Master, zu dem der Slave geh�rt. */

    // Addresses
    uint16_t ring_position; /**< Position des Slaves im Bus */
    uint16_t station_address; /**< Konfigurierte Slave-Adresse */

    // Base data
    uint8_t base_type; /**< Slave-Typ */
    uint8_t base_revision; /**< Revision */
    uint16_t base_build; /**< Build-Nummer */
    uint16_t base_fmmu_count; /**< Anzahl unterst�tzter FMMUs */
    uint16_t base_sync_count; /**< Anzahl unterst�tzter Sync-Manager */

    // Slave information interface
    uint32_t sii_vendor_id; /**< Identifikationsnummer des Herstellers */
    uint32_t sii_product_code; /**< Herstellerspezifischer Produktcode */
    uint32_t sii_revision_number; /**< Revisionsnummer */
    uint32_t sii_serial_number; /**< Seriennummer der Klemme */

    const ec_slave_type_t *type; /**< Zeiger auf die Beschreibung
                                    des Slave-Typs */

    uint8_t registered; /**< Der Slave wurde registriert */

    ec_fmmu_t fmmus[EC_MAX_FMMUS]; /**< FMMU-Konfigurationen */
    uint8_t fmmu_count; /**< Wieviele FMMUs schon benutzt sind. */
};

/*****************************************************************************/

// Slave construction/destruction
void ec_slave_init(ec_slave_t *, ec_master_t *);
void ec_slave_clear(ec_slave_t *);

// Slave control
int ec_slave_fetch(ec_slave_t *);
int ec_slave_sii_read(ec_slave_t *, unsigned short, unsigned int *);
int ec_slave_state_change(ec_slave_t *, uint8_t);
int ec_slave_set_fmmu(ec_slave_t *, const ec_domain_t *, const ec_sync_t *);

// Misc
void ec_slave_print(const ec_slave_t *);
int ec_slave_check_crc(ec_slave_t *);

/*****************************************************************************/

#endif

/* Emacs-Konfiguration
;;; Local Variables: ***
;;; c-basic-offset:4 ***
;;; End: ***
*/
