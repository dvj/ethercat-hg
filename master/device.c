/******************************************************************************
 *
 *  d e v i c e . c
 *
 *  Methoden f�r ein EtherCAT-Ger�t.
 *
 *  $Id$
 *
 *****************************************************************************/

#include <linux/module.h>
#include <linux/skbuff.h>
#include <linux/if_ether.h>
#include <linux/netdevice.h>
#include <linux/delay.h>

#include "device.h"
#include "master.h"

/*****************************************************************************/

/**
   EtherCAT-Ger�te-Konstuktor.

   \return 0 wenn alles ok, < 0 bei Fehler (zu wenig Speicher)
*/

int ec_device_init(ec_device_t *device, /**< EtherCAT-Ger�t */
                   ec_master_t *master /**< Zugeh�riger Master */
                   )
{
    device->master = master;
    device->dev = NULL;
    device->open = 0;
    device->tx_time = 0;
    device->rx_time = 0;
    device->state = EC_DEVICE_STATE_READY;
    device->rx_data_size = 0;
    device->isr = NULL;
    device->module = NULL;
    device->error_reported = 0;
    device->link_state = 0; // down

    if ((device->tx_skb = dev_alloc_skb(ETH_HLEN + EC_MAX_FRAME_SIZE)) == NULL) {
        EC_ERR("Error allocating device socket buffer!\n");
        return -1;
    }

    return 0;
}

/*****************************************************************************/

/**
   EtherCAT-Ger�te-Destuktor.

   Gibt den dynamisch allozierten Speicher des
   EtherCAT-Ger�tes (den Sende-Socket-Buffer) wieder frei.
*/

void ec_device_clear(ec_device_t *device /**< EtherCAT-Ger�t */)
{
    if (device->open) ec_device_close(device);

    device->dev = NULL;

    if (device->tx_skb) {
        dev_kfree_skb(device->tx_skb);
        device->tx_skb = NULL;
    }
}

/*****************************************************************************/

/**
   F�hrt die open()-Funktion des Netzwerktreibers aus.

   Dies entspricht einem "ifconfig up". Vorher wird der Zeiger
   auf das EtherCAT-Ger�t auf G�ltigkeit gepr�ft und der
   Ger�tezustand zur�ckgesetzt.

   \return 0 bei Erfolg, < 0: Ung�ltiger Zeiger, oder open()
           fehlgeschlagen
*/

int ec_device_open(ec_device_t *device /**< EtherCAT-Ger�t */)
{
    unsigned int i;

    if (!device->dev) {
        EC_ERR("No net_device to open!\n");
        return -1;
    }

    if (device->open) {
        EC_WARN("Device already opened!\n");
        return 0;
    }

    // Device could have received frames before
    for (i = 0; i < 4; i++) ec_device_call_isr(device);

    // Reset old device state
    device->state = EC_DEVICE_STATE_READY;
    device->link_state = 0;

    if (device->dev->open(device->dev) == 0) device->open = 1;

    return device->open ? 0 : -1;
}

/*****************************************************************************/

/**
   F�hrt die stop()-Funktion des net_devices aus.

   \return 0 bei Erfolg, < 0: Kein Ger�t zum Schlie�en oder
           Schlie�en fehlgeschlagen.
*/

int ec_device_close(ec_device_t *device /**< EtherCAT-Ger�t */)
{
    if (!device->dev) {
        EC_ERR("No device to close!\n");
        return -1;
    }

    if (!device->open) {
        EC_WARN("Device already closed!\n");
    }
    else {
        if (device->dev->stop(device->dev) == 0) device->open = 0;
    }

    return !device->open ? 0 : -1;
}

/*****************************************************************************/

/**
   Bereitet den ger�teinternen Socket-Buffer auf den Versand vor.

   \return Zeiger auf den Speicher, in den die Frame-Daten sollen.
*/

uint8_t *ec_device_prepare(ec_device_t *device /**< EtherCAT-Ger�t */)
{
    skb_trim(device->tx_skb, 0); // Auf L�nge 0 abschneiden
    skb_reserve(device->tx_skb, ETH_HLEN); // Reserve f�r Ethernet-II-Header

    // Vorerst Speicher f�r maximal langen Frame reservieren
    return skb_put(device->tx_skb, EC_MAX_FRAME_SIZE);
}

/*****************************************************************************/

/**
   Sendet den Inhalt des Socket-Buffers.

   Schneidet den Inhalt des Socket-Buffers auf die (nun bekannte) Gr��e zu,
   f�gt den Ethernet-II-Header an und ruft die start_xmit()-Funktion der
   Netzwerkkarte auf.
*/

void ec_device_send(ec_device_t *device, /**< EtherCAT-Ger�t */
                    unsigned int length /**< L�nge der zu sendenden Daten */
                    )
{
    struct ethhdr *eth;

    if (unlikely(!device->link_state)) { // Link down
        return;
    }

    // Framegr��e auf (jetzt bekannte) L�nge abschneiden
    skb_trim(device->tx_skb, length);

    // Ethernet-II-Header hinzufuegen
    eth = (struct ethhdr *) skb_push(device->tx_skb, ETH_HLEN);
    eth->h_proto = htons(0x88A4);
    memcpy(eth->h_source, device->dev->dev_addr, device->dev->addr_len);
    memset(eth->h_dest, 0xFF, device->dev->addr_len);

    device->state = EC_DEVICE_STATE_SENT;
    device->rx_data_size = 0;

    if (unlikely(device->master->debug_level > 1)) {
        EC_DBG("Sending frame:\n");
        ec_data_print(device->tx_skb->data + ETH_HLEN, device->tx_skb->len);
    }

    // Senden einleiten
    rdtscl(device->tx_time); // Get CPU cycles
    device->dev->hard_start_xmit(device->tx_skb, device->dev);
}

/*****************************************************************************/

/**
   Gibt die Anzahl der empfangenen Bytes zur�ck.

   \return Empfangene Bytes, oder 0, wenn kein Frame empfangen wurde.
*/

unsigned int ec_device_received(const ec_device_t *device)
{
    return device->rx_data_size;
}

/*****************************************************************************/

/**
   Gibt die empfangenen Daten zur�ck.

   \return Zeiger auf empfangene Daten.
*/

uint8_t *ec_device_data(ec_device_t *device)
{
    return device->rx_data;
}

/*****************************************************************************/

/**
   Ruft die Interrupt-Routine der Netzwerkkarte auf.
*/

void ec_device_call_isr(ec_device_t *device /**< EtherCAT-Ger�t */)
{
    if (likely(device->isr)) device->isr(0, device->dev, NULL);
}

/*****************************************************************************/

/**
   Gibt alle Informationen �ber das Device-Objekt aus.
*/

void ec_device_print(ec_device_t *device /**< EtherCAT-Ger�t */)
{
    EC_DBG("---EtherCAT device information begin---\n");

    if (device) {
        EC_DBG("Assigned net_device: %X\n", (u32) device->dev);
        EC_DBG("Transmit socket buffer: %X\n", (u32) device->tx_skb);
        EC_DBG("Time of last transmission: %u\n", (u32) device->tx_time);
        EC_DBG("Time of last receive: %u\n", (u32) device->rx_time);
        EC_DBG("Actual device state: %i\n", (u8) device->state);
        EC_DBG("Receive buffer: %X\n", (u32) device->rx_data);
        EC_DBG("Receive buffer fill state: %u/%u\n",
               (u32) device->rx_data_size, EC_MAX_FRAME_SIZE);
    }
    else {
        EC_DBG("Device is NULL!\n");
    }

    EC_DBG("---EtherCAT device information end---\n");
}

/*****************************************************************************/

/**
   Gibt das letzte Rahmenpaar aus.
*/

void ec_device_debug(const ec_device_t *device /**< EtherCAT-Ger�t */)
{
    EC_DBG(">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n");
    ec_data_print(device->tx_skb->data + ETH_HLEN, device->tx_skb->len);
    EC_DBG("--------------------------------------\n");
    ec_data_print_diff(device->tx_skb->data + ETH_HLEN, device->rx_data,
                       device->rx_data_size);
    EC_DBG("<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<\n");
}

/*****************************************************************************/

/**
   Gibt Frame-Inhalte zwecks Debugging aus.
*/

void ec_data_print(const uint8_t *data /**< Daten */,
                   size_t size /**< Anzahl Bytes */
                   )
{
    size_t i;

    EC_DBG("");
    for (i = 0; i < size; i++) {
        printk("%02X ", data[i]);
        if ((i + 1) % 16 == 0) {
            printk("\n");
            EC_DBG("");
        }
    }
    printk("\n");
}

/*****************************************************************************/

/**
   Gibt Frame-Inhalte zwecks Debugging aus, differentiell.
*/

void ec_data_print_diff(const uint8_t *d1, /**< Daten 1 */
                        const uint8_t *d2, /**< Daten 2 */
                        size_t size /** Anzahl Bytes */
                        )
{
    size_t i;

    EC_DBG("");
    for (i = 0; i < size; i++) {
        if (d1[i] == d2[i]) printk(".. ");
        else printk("%02X ", d2[i]);
        if ((i + 1) % 16 == 0) {
            printk("\n");
            EC_DBG("");
        }
    }
    printk("\n");
}

/******************************************************************************
 *
 * Treiberschnittstelle
 *
 *****************************************************************************/

/**
   Setzt den Zustand des EtherCAT-Ger�tes.
*/

void EtherCAT_dev_state(ec_device_t *device,  /**< EtherCAT-Ger�t */
                        ec_device_state_t state /**< Neuer Zustand */
                        )
{
    device->state = state;
}

/*****************************************************************************/

/**
   Pr�ft, ob das Net-Device \a dev zum registrierten EtherCAT-Ger�t geh�rt.

   \return 0 wenn nein, nicht-null wenn ja.
*/

int EtherCAT_dev_is_ec(const ec_device_t *device,  /**< EtherCAT-Ger�t */
                       const struct net_device *dev /**< Net-Device */
                       )
{
    return device && device->dev == dev;
}

/*****************************************************************************/

/**
   Nimmt einen Empfangenen Rahmen entgegen.

   Kopiert die empfangenen Daten in den Receive-Buffer.
*/

void EtherCAT_dev_receive(ec_device_t *device, /**< EtherCAT-Ger�t */
                          const void *data, /**< Zeiger auf empfangene Daten */
                          size_t size /**< Gr��e der empfangenen Daten */
                          )
{
    // Copy received data to ethercat-device buffer
    memcpy(device->rx_data, data, size);
    device->rx_data_size = size;
    device->state = EC_DEVICE_STATE_RECEIVED;

    if (unlikely(device->master->debug_level > 1)) {
        EC_DBG("Received frame:\n");
        ec_data_print_diff(device->tx_skb->data + ETH_HLEN, device->rx_data,
                           device->rx_data_size);
    }
}

/*****************************************************************************/

/**
   Setzt einen neuen Verbindungszustand.
*/

void EtherCAT_dev_link_state(ec_device_t *device, /**< EtherCAT-Ger�t */
                             uint8_t state /**< Verbindungszustand */
                             )
{
    if (state != device->link_state) {
        device->link_state = state;
        EC_INFO("Link state changed to %s.\n", (state ? "UP" : "DOWN"));
    }
}

/*****************************************************************************/

EXPORT_SYMBOL(EtherCAT_dev_is_ec);
EXPORT_SYMBOL(EtherCAT_dev_state);
EXPORT_SYMBOL(EtherCAT_dev_receive);
EXPORT_SYMBOL(EtherCAT_dev_link_state);

/*****************************************************************************/

/* Emacs-Konfiguration
;;; Local Variables: ***
;;; c-basic-offset:4 ***
;;; End: ***
*/
