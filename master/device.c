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

void ec_data_print(const uint8_t *, size_t);
void ec_data_print_diff(const uint8_t *, const uint8_t *, size_t);

/*****************************************************************************/

/**
   EtherCAT-Ger�te-Konstuktor.

   \return 0 wenn alles ok, < 0 bei Fehler (zu wenig Speicher)
*/

int ec_device_init(ec_device_t *device, /**< EtherCAT-Ger�t */
                   ec_master_t *master, /**< Zugeh�riger Master */
                   struct net_device *net_dev, /**< Net-Device */
                   ec_isr_t isr, /**< Adresse der ISR */
                   struct module *module /**< Modul-Adresse */
                   )
{
    struct ethhdr *eth;

    device->master = master;
    device->dev = net_dev;
    device->isr = isr;
    device->module = module;

    device->open = 0;
    device->link_state = 0; // down

    if (!(device->tx_skb = dev_alloc_skb(ETH_HLEN + EC_MAX_FRAME_SIZE))) {
        EC_ERR("Error allocating device socket buffer!\n");
        return -1;
    }

    device->tx_skb->dev = net_dev;

    // Ethernet-II-Header hinzufuegen
    skb_reserve(device->tx_skb, ETH_HLEN);
    eth = (struct ethhdr *) skb_push(device->tx_skb, ETH_HLEN);
    eth->h_proto = htons(0x88A4);
    memcpy(eth->h_source, net_dev->dev_addr, net_dev->addr_len);
    memset(eth->h_dest, 0xFF, net_dev->addr_len);

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
    if (device->tx_skb) dev_kfree_skb(device->tx_skb);
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
        return 0;
    }

    if (device->dev->stop(device->dev) == 0) device->open = 0;

    return !device->open ? 0 : -1;
}

/*****************************************************************************/

/**
   Liefert einen Zeiger auf den Sende-Speicher.

   \return Zeiger auf den Speicher, in den die Frame-Daten sollen.
*/

uint8_t *ec_device_tx_data(ec_device_t *device /**< EtherCAT-Ger�t */)
{
    return device->tx_skb->data + ETH_HLEN;
}

/*****************************************************************************/

/**
   Sendet den Inhalt des Socket-Buffers.

   Schneidet den Inhalt des Socket-Buffers auf die (nun bekannte) Gr��e zu,
   f�gt den Ethernet-II-Header an und ruft die start_xmit()-Funktion der
   Netzwerkkarte auf.
*/

void ec_device_send(ec_device_t *device, /**< EtherCAT-Ger�t */
                    size_t size /**< Gr��e der zu sendenden Daten */
                    )
{
    if (unlikely(!device->link_state)) // Link down
        return;

    // Framegr��e auf (jetzt bekannte) L�nge abschneiden
    device->tx_skb->len = ETH_HLEN + size;

    if (unlikely(device->master->debug_level > 1)) {
        EC_DBG("sending frame:\n");
        ec_data_print(device->tx_skb->data + ETH_HLEN, size);
    }

    // Senden einleiten
    device->dev->hard_start_xmit(device->tx_skb, device->dev);
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
   Pr�ft, ob das Net-Device \a dev zum registrierten EtherCAT-Ger�t geh�rt.

   \return 0 wenn nein, nicht-null wenn ja.
*/

inline int ecdev_is_ec(const ec_device_t *device,  /**< EtherCAT-Ger�t */
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

void ecdev_receive(ec_device_t *device, /**< EtherCAT-Ger�t */
                   const void *data, /**< Zeiger auf empfangene Daten */
                   size_t size /**< Gr��e der empfangenen Daten */
                   )
{
    if (unlikely(device->master->debug_level > 1)) {
        EC_DBG("Received frame:\n");
        ec_data_print_diff(device->tx_skb->data + ETH_HLEN, data, size);
    }

    ec_master_receive(device->master, data, size);
}

/*****************************************************************************/

/**
   Setzt einen neuen Verbindungszustand.
*/

void ecdev_link_state(ec_device_t *device, /**< EtherCAT-Ger�t */
                      uint8_t state /**< Verbindungszustand */
                      )
{
    if (likely(state != device->link_state)) {
        device->link_state = state;
        EC_INFO("Link state changed to %s.\n", (state ? "UP" : "DOWN"));
    }
}

/*****************************************************************************/

EXPORT_SYMBOL(ecdev_is_ec);
EXPORT_SYMBOL(ecdev_receive);
EXPORT_SYMBOL(ecdev_link_state);

/*****************************************************************************/

/* Emacs-Konfiguration
;;; Local Variables: ***
;;; c-basic-offset:4 ***
;;; End: ***
*/
