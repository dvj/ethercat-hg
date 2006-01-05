/******************************************************************************
 *
 *  e c _ d o m a i n . c
 *
 *  Methoden f�r Gruppen von EtherCAT-Slaves.
 *
 *  $Id$
 *
 *****************************************************************************/

#include <linux/module.h>

#include "ec_globals.h"
#include "ec_domain.h"

/*****************************************************************************/

/**
   Konstruktor einer EtherCAT-Dom�ne.

   @param pd Zeiger auf die zu initialisierende Dom�ne
*/

void EtherCAT_domain_init(EtherCAT_domain_t *dom)
{
  dom->number = 0;
  dom->data = NULL;
  dom->data_size = 0;
  dom->logical_offset = 0;
}

/*****************************************************************************/

/**
   Destruktor eines Prozessdatenobjekts.

   @param dom Zeiger auf die zu l�schenden Prozessdaten
*/

void EtherCAT_domain_clear(EtherCAT_domain_t *dom)
{
  if (dom->data) {
    kfree(dom->data);
    dom->data = NULL;
  }

  dom->data_size = 0;
}

/*****************************************************************************/

/* Emacs-Konfiguration
;;; Local Variables: ***
;;; c-basic-offset:2 ***
;;; End: ***
*/
