/******************************************************************************
 *
 *  e c _ s l a v e . c
 *
 *  Methoden f�r einen EtherCAT-Slave.
 *
 *  $Id$
 *
 *****************************************************************************/

#include <linux/module.h>

#include "ec_globals.h"
#include "ec_slave.h"

/*****************************************************************************/

/**
   EtherCAT-Slave-Konstruktor.

   Initialisiert einen EtherCAT-Slave.

   ACHTUNG! Dieser Konstruktor wird quasi nie aufgerufen. Bitte immer das
   Makro ECAT_INIT_SLAVE() in ec_slave.h anpassen!

   @param slave Zeiger auf den zu initialisierenden Slave
*/

void EtherCAT_slave_init(EtherCAT_slave_t *slave)
{
  slave->type = 0;
  slave->revision = 0;
  slave->build = 0;
  slave->ring_position = 0;
  slave->station_address = 0;
  slave->vendor_id = 0;
  slave->product_code = 0;
  slave->revision_number = 0;
  slave->serial_number = 0;
  slave->desc = NULL;
  slave->logical_address = 0;
  slave->current_state = ECAT_STATE_UNKNOWN;
  slave->requested_state = ECAT_STATE_UNKNOWN;
  slave->process_data = NULL;
  slave->domain = 0;
  slave->error_reported = 0;
}

/*****************************************************************************/

/**
   Liest einen bestimmten Kanal des Slaves als Integer-Wert.

   Pr�ft zuerst, ob der entsprechende Slave eine
   bekannte Beschreibung besitzt, ob dort eine
   read()-Funktion hinterlegt ist und ob die angegebene
   Kanalnummer g�ltig ist. Wenn ja, wird der dekodierte
   Wert zur�ckgegeben, sonst ist der Wert 0.

   @param slave EtherCAT-Slave
   @param channel Kanalnummer

   @return Gelesener Wert bzw. 0
*/

int EtherCAT_read_value(EtherCAT_slave_t *slave,
                        unsigned int channel)
{
  if (unlikely(!slave->desc)) {
    if (likely(slave->error_reported)) {
      printk(KERN_WARNING "EtherCAT: Reading failed on slave %04X (addr %0X)"
             " - Slave has no description.\n",
             slave->station_address, (unsigned int) slave);
      slave->error_reported = 1;
    }
    return 0;
  }

  if (unlikely(!slave->desc->read)) {
    if (likely(slave->error_reported)) {
      printk(KERN_WARNING "EtherCAT: Reading failed on slave %04X (addr %0X)"
             " - Slave type (%s %s) has no read method.\n",
             slave->station_address, (unsigned int) slave,
             slave->desc->vendor_name, slave->desc->product_name);
      slave->error_reported = 1;
    }
    return 0;
  }

  if (unlikely(channel >= slave->desc->channel_count)) {
    if (likely(slave->error_reported)) {
      printk(KERN_WARNING "EtherCAT: Reading failed on slave %4X (addr %0X)"
             " - Type (%s %s) has no channel %i.\n",
             slave->station_address, (unsigned int) slave,
             slave->desc->vendor_name, slave->desc->product_name,
             channel);
      slave->error_reported = 1;
    }
    return 0;
  }

  if (unlikely(!slave->process_data)) {
    if (likely(slave->error_reported)) {
      printk(KERN_WARNING "EtherCAT: Reading failed on slave %4X (addr %0X)"
             " - Slave does not belong to any process data object!\n",
             slave->station_address, (unsigned int) slave);
      slave->error_reported = 1;
    }
    return 0;
  }

  if (unlikely(slave->error_reported))
    slave->error_reported = 0;

  return slave->desc->read(slave->process_data, channel);
}

/*****************************************************************************/

/**
   Schreibt einen bestimmten Kanal des Slaves als Integer-Wert .

   Pr�ft zuerst, ob der entsprechende Slave eine
   bekannte Beschreibung besitzt, ob dort eine
   write()-Funktion hinterlegt ist und ob die angegebene
   Kanalnummer g�ltig ist. Wenn ja, wird der Wert entsprechend
   kodiert und geschrieben.

   @param slave EtherCAT-Slave
   @param channel Kanalnummer
   @param value Zu schreibender Wert
*/

void EtherCAT_write_value(EtherCAT_slave_t *slave,
                          unsigned int channel,
                          int value)
{
  if (unlikely(!slave->desc)) {
    if (likely(slave->error_reported)) {
      printk(KERN_WARNING "EtherCAT: Writing failed on slave %04X (addr %0X)"
             " - Slave has no description.\n",
             slave->station_address, (unsigned int) slave);
      slave->error_reported = 1;
    }
    return;
  }

  if (unlikely(!slave->desc->write)) {
    if (likely(slave->error_reported)) {
      printk(KERN_WARNING "EtherCAT: Writing failed on slave %04X (addr %0X)"
             " - Type (%s %s) has no write method.\n",
             slave->station_address, (unsigned int) slave,
             slave->desc->vendor_name, slave->desc->product_name);
      slave->error_reported = 1;
    }
    return;
  }

  if (unlikely(channel >= slave->desc->channel_count)) {
    if (likely(slave->error_reported)) {
      printk(KERN_WARNING "EtherCAT: Writing failed on slave %4X (addr %0X)"
             " - Type (%s %s) has no channel %i.\n",
             slave->station_address, (unsigned int) slave,
             slave->desc->vendor_name, slave->desc->product_name,
             channel);
      slave->error_reported = 1;
    }
    return;
  }

  if (unlikely(!slave->process_data)) {
    if (likely(slave->error_reported)) {
      printk(KERN_WARNING "EtherCAT: Writing failed on slave %4X (addr %0X)"
             " - Slave does not belong to any process data object!\n",
             slave->station_address, (unsigned int) slave);
      slave->error_reported = 1;
    }
    return;
  }

  if (unlikely(slave->error_reported))
    slave->error_reported = 0;

  slave->desc->write(slave->process_data, channel, value);
}

/*****************************************************************************/

EXPORT_SYMBOL(EtherCAT_write_value);
EXPORT_SYMBOL(EtherCAT_read_value);

/*****************************************************************************/

/* Emacs-Konfiguration
;;; Local Variables: ***
;;; c-basic-offset:2 ***
;;; End: ***
*/
