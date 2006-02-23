/******************************************************************************
 *
 *  d e v i c e . h
 *
 *  Struktur f�r ein EtherCAT-Ger�t.
 *
 *  $Id$
 *
 *****************************************************************************/

#ifndef _EC_DEVICE_H_
#define _EC_DEVICE_H_

#include <linux/interrupt.h>

#include "globals.h"
#include "../include/EtherCAT_dev.h"

/*****************************************************************************/

/**
   EtherCAT-Ger�t.

   Ein EtherCAT-Ger�t ist eine Netzwerkkarte, die vom
   EtherCAT-Master dazu verwendet wird, um Frames zu senden
   und zu empfangen.
*/

struct ec_device
{
    struct net_device *dev; /**< Zeiger auf das reservierte net_device */
    unsigned int open;      /**< Das net_device ist geoeffnet. */
    struct sk_buff *tx_skb; /**< Zeiger auf Transmit-Socketbuffer */
    unsigned long tx_time;  /**< Zeit des letzten Sendens */
    unsigned long rx_time;  /**< Zeit des letzten Empfangs */
    volatile ec_device_state_t state; /**< Zustand des Ger�tes */
    uint8_t rx_data[EC_MAX_FRAME_SIZE]; /**< Speicher f�r empfangene Rahmen */
    volatile unsigned int rx_data_length; /**< L�nge des empfangenen Rahmens */
    irqreturn_t (*isr)(int, void *, struct pt_regs *); /**< Adresse der ISR */
    struct module *module; /**< Zeiger auf das Modul, das das Ger�t zur
                              Verf�gung stellt. */
    int error_reported; /**< Zeigt an, ob ein Fehler im zyklischen Code
                           bereits gemeldet wurde. */
};

/*****************************************************************************/

int ec_device_init(ec_device_t *);
void ec_device_clear(ec_device_t *);
int ec_device_open(ec_device_t *);
int ec_device_close(ec_device_t *);
void ec_device_call_isr(ec_device_t *);
uint8_t *ec_device_prepare(ec_device_t *);
void ec_device_send(ec_device_t *, unsigned int);
unsigned int ec_device_received(const ec_device_t *);
uint8_t *ec_device_data(ec_device_t *);

/*****************************************************************************/

#endif

/* Emacs-Konfiguration
;;; Local Variables: ***
;;; c-basic-offset:4 ***
;;; End: ***
*/
