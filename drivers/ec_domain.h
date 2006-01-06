/******************************************************************************
 *
 *  e c _ d o m a i n . h
 *
 *  Struktur f�r eine Gruppe von EtherCAT-Slaves.
 *
 *  $Id$
 *
 *****************************************************************************/

#ifndef _EC_DOMAIN_H_
#define _EC_DOMAIN_H_

#include "ec_globals.h"
#include "ec_slave.h"
#include "ec_command.h"

/*****************************************************************************/

/**
   EtherCAT-Dom�ne

   Verwaltet die Prozessdaten und das hierf�r n�tige Kommando einer bestimmten
   Menge von Slaves.
*/

typedef struct EtherCAT_domain
{
  unsigned int number; /*<< Dom�nen-Identifikation */
  EtherCAT_command_t command; /**< Kommando zum Senden und Empfangen der
                                 Prozessdaten */
  unsigned char *data; /**< Zeiger auf Speicher mit Prozessdaten */
  unsigned int data_size; /**< Gr��e des Prozessdatenspeichers */
  unsigned int logical_offset; /**< Logische Basisaddresse */
  unsigned int response_count; /**< Anzahl antwortender Slaves */
}
EtherCAT_domain_t;

/*****************************************************************************/

void EtherCAT_domain_init(EtherCAT_domain_t *);
void EtherCAT_domain_clear(EtherCAT_domain_t *);

/*****************************************************************************/

#endif

/* Emacs-Konfiguration
;;; Local Variables: ***
;;; c-basic-offset:2 ***
;;; End: ***
*/
