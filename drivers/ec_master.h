/******************************************************************************
 *
 *  e c _ m a s t e r . h
 *
 *  Struktur f�r einen EtherCAT-Master.
 *
 *  $Id$
 *
 *****************************************************************************/

#ifndef _EC_MASTER_H_
#define _EC_MASTER_H_

#include "ec_device.h"
#include "ec_slave.h"
#include "ec_command.h"
#include "ec_domain.h"

/*****************************************************************************/

/**
   EtherCAT-Master

   Verwaltet die EtherCAT-Slaves und kommuniziert mit
   dem zugewiesenen EtherCAT-Ger�t.
*/

struct EtherCAT_master
{
  EtherCAT_device_t *dev; /**< Zeiger auf das zugewiesene EtherCAT-Ger�t */
  unsigned char command_index; /**< Aktueller Kommando-Index */
  unsigned char tx_data[ECAT_FRAME_BUFFER_SIZE]; /**< Statischer Speicher
                                                    f�r zu sendende Daten */
  unsigned int tx_data_length; /**< L�nge der Daten im Sendespeicher */
  unsigned char rx_data[ECAT_FRAME_BUFFER_SIZE]; /**< Statische Speicher f�r
                                                    eine Kopie des Rx-Buffers
                                                    im EtherCAT-Ger�t */
  unsigned int rx_data_length; /**< L�nge der Daten im Empfangsspeicher */
  EtherCAT_domain_t domains[ECAT_MAX_DOMAINS]; /** Prozessdatendom�nen */
  unsigned int domain_count;
  int debug_level; /**< Debug-Level im Master-Code */
  unsigned long tx_time; /**< Zeit des letzten Sendens */
  unsigned long rx_time; /**< Zeit des letzten Empfangs */
  unsigned int rx_tries; /**< Anzahl Warteschleifen beim letzen Enpfang */
};

/*****************************************************************************/

// Master creation and deletion
void EtherCAT_master_init(EtherCAT_master_t *);
void EtherCAT_master_clear(EtherCAT_master_t *);

// Registration of devices
int EtherCAT_master_open(EtherCAT_master_t *, EtherCAT_device_t *);
void EtherCAT_master_close(EtherCAT_master_t *, EtherCAT_device_t *);

// Sending and receiving
int EtherCAT_simple_send_receive(EtherCAT_master_t *, EtherCAT_command_t *);
int EtherCAT_simple_send(EtherCAT_master_t *, EtherCAT_command_t *);
int EtherCAT_simple_receive(EtherCAT_master_t *, EtherCAT_command_t *);

// Slave management
int EtherCAT_check_slaves(EtherCAT_master_t *, EtherCAT_slave_t *,
                          unsigned int);
int EtherCAT_read_slave_information(EtherCAT_master_t *, unsigned short int,
                                    unsigned short int, unsigned int *);
int EtherCAT_activate_slave(EtherCAT_master_t *, EtherCAT_slave_t *);
int EtherCAT_deactivate_slave(EtherCAT_master_t *, EtherCAT_slave_t *);
int EtherCAT_state_change(EtherCAT_master_t *, EtherCAT_slave_t *,
                          unsigned char);

// Process data
int EtherCAT_process_data_cycle(EtherCAT_master_t *, unsigned int);

// Private functions
void output_debug_data(const EtherCAT_master_t *);

/*****************************************************************************/

#endif

/* Emacs-Konfiguration
;;; Local Variables: ***
;;; c-basic-offset:2 ***
;;; End: ***
*/
