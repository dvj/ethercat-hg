/******************************************************************************
 *
 *  c a n o p e n . c
 *
 *  CANopen over EtherCAT
 *
 *  $Id$
 *
 *****************************************************************************/

#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/module.h>

#include "master.h"

/*****************************************************************************/

/**
   SDO Abort Code Messages
*/

typedef struct
{
    uint32_t code;
    const char *message;
}
ec_sdo_abort_message_t;

const ec_sdo_abort_message_t sdo_abort_messages[];

void ec_canopen_abort_msg(uint32_t);

/*****************************************************************************/

/**
   Schreibt ein CANopen-SDO (service data object).
 */

int ecrt_slave_sdo_write(ec_slave_t *slave, /**< EtherCAT-Slave */
                         uint16_t sdo_index, /**< SDO-Index */
                         uint8_t sdo_subindex, /**< SDO-Subindex */
                         uint32_t value, /**< Neuer Wert */
                         size_t size /**< Gr��e des Datenfeldes */
                         )
{
    uint8_t data[0x0A];
    unsigned int i;
    size_t rec_size;

    if (size == 0 || size > 4) {
        EC_ERR("Invalid SDO data size: %i!\n", size);
        return -1;
    }

    EC_WRITE_U16(data,     0x02 << 12); // Number (0), Service (SDO request)
    EC_WRITE_U8 (data + 2, 0x23 | ((4 - size) << 2)); // Spec., exp., init.
    EC_WRITE_U16(data + 3, sdo_index);
    EC_WRITE_U8 (data + 5, sdo_subindex);

    for (i = 0; i < size; i++) {
        EC_WRITE_U8(data + 6 + i, value & 0xFF);
        value >>= 8;
    }

    // Mailox senden und empfangen
    if (ec_slave_mailbox_send(slave, 0x03, data, 0x0A)) return -1;

    rec_size = 0x0A;
    if (ec_slave_mailbox_receive(slave, 0x03, data, &rec_size)) return -1;

    if (EC_READ_U16(data) >> 12 == 0x02 && // SDO request
        EC_READ_U8 (data + 2) >> 5 == 0x04) { // Abort SDO transf. req.
        EC_ERR("SDO download of 0x%04X:%X (value %X, size %X) aborted on slave"
               " %i.\n", sdo_index, sdo_subindex, value, size,
               slave->ring_position);
        ec_canopen_abort_msg(EC_READ_U32(data + 6));
        return -1;
    }

    if (EC_READ_U16(data) >> 12 != 0x03 || // SDO response
        EC_READ_U8 (data + 2) >> 5 != 0x03 || // Download response
        EC_READ_U16(data + 3) != sdo_index || // Index
        EC_READ_U8 (data + 5) != sdo_subindex) // Subindex
    {
        EC_ERR("Invalid SDO download response at slave %i!\n",
               slave->ring_position);
        return -1;
    }

    return 0;
}

/*****************************************************************************/

/**
   Liest ein CANopen-SDO (service data object).
 */

int ecrt_slave_sdo_read(ec_slave_t *slave, /**< EtherCAT-Slave */
                        uint16_t sdo_index, /**< SDO-Index */
                        uint8_t sdo_subindex, /**< SDO-Subindex */
                        uint32_t *value /**< Speicher f�r gel. Wert */
                        )
{
    uint8_t data[0x20];
    size_t rec_size;

    EC_WRITE_U16(data,     0x2000); // Number (0), Service = SDO request
    EC_WRITE_U8 (data + 2, 0x1 << 1 | 0x2 << 5); // Expedited upload request
    EC_WRITE_U16(data + 3, sdo_index);
    EC_WRITE_U8 (data + 5, sdo_subindex);

    if (ec_slave_mailbox_send(slave, 0x03, data, 6)) return -1;

    rec_size = 0x20;
    if (ec_slave_mailbox_receive(slave, 0x03, data, &rec_size)) return -1;

    if (EC_READ_U16(data) >> 12 == 0x02 && // SDO request
        EC_READ_U8 (data + 2) >> 5 == 0x04) { // Abort SDO transfer request
        EC_ERR("SDO upload of 0x%04X:%X aborted on slave %i.\n",
               sdo_index, sdo_subindex, slave->ring_position);
        ec_canopen_abort_msg(EC_READ_U32(data + 6));
        return -1;
    }

    if (EC_READ_U16(data) >> 12 != 0x03 || // SDO response
        EC_READ_U8 (data + 2) >> 5 != 0x02 || // Upload response
        EC_READ_U16(data + 3) != sdo_index || // Index
        EC_READ_U8 (data + 5) != sdo_subindex) { // Subindex
        EC_ERR("Invalid SDO upload response at slave %i!\n",
               slave->ring_position);
        return -1;
    }

    *value = EC_READ_U32(data + 6);
    return 0;
}

/*****************************************************************************/

/**
   Schweibt ein CANopen-SDO (Variante mit Angabe des Masters und der Adresse).

   Siehe ecrt_slave_sdo_write()

   \return 0 wenn alles ok, < 0 bei Fehler
 */

int ecrt_master_sdo_write(ec_master_t *master,
                          /**< EtherCAT-Master */
                          const char *addr,
                          /**< Addresse, siehe ec_master_slave_address() */
                          uint16_t index,
                          /**< SDO-Index */
                          uint8_t subindex,
                          /**< SDO-Subindex */
                          uint32_t value,
                          /**< Neuer Wert */
                          size_t size
                          /**< Gr��e des Datenfeldes */
                          )
{
    ec_slave_t *slave;
    if (!(slave = ec_master_slave_address(master, addr))) return -1;
    return ecrt_slave_sdo_write(slave, index, subindex, value, size);
}

/*****************************************************************************/

/**
   Liest ein CANopen-SDO (Variante mit Angabe des Masters und der Adresse).

   Siehe ecrt_slave_sdo_read()

   \return 0 wenn alles ok, < 0 bei Fehler
 */

int ecrt_master_sdo_read(ec_master_t *master,
                         /**< EtherCAT-Slave */
                         const char *addr,
                         /**< Addresse, siehe ec_master_slave_address() */
                         uint16_t index,
                         /**< SDO-Index */
                         uint8_t subindex,
                         /**< SDO-Subindex */
                         uint32_t *value
                         /**< Speicher f�r gel. Wert */
                         )
{
    ec_slave_t *slave;
    if (!(slave = ec_master_slave_address(master, addr))) return -1;
    return ecrt_slave_sdo_read(slave, index, subindex, value);
}

/*****************************************************************************/

/**
   Holt das Object-Dictionary aus dem Slave.

   \return 0, wenn alles ok, sonst < 0
*/

int ec_slave_fetch_sdo_list(ec_slave_t *slave /**< EtherCAT-Slave */)
{
    uint8_t data[0xF0];
    size_t rec_size;

    //EC_DBG("Fetching SDO list for slave %i...\n", slave->ring_position);

    EC_WRITE_U16(data,     0x8000); // Number (0), Service (get OD request)
    EC_WRITE_U8 (data + 2,   0x01); // Get OD List Request
    EC_WRITE_U8 (data + 3,   0x00); // res.
    EC_WRITE_U16(data + 4, 0x0000); // fragments left
    EC_WRITE_U16(data + 6, 0x0001); // Deliver all SDOs!

    if (ec_slave_mailbox_send(slave, 0x03, data, 8)) return -1;

    do
    {
        rec_size = 0xF0;
        if (ec_slave_mailbox_receive(slave, 0x03, data, &rec_size)) return -1;

        if (EC_READ_U16(data) >> 12 == 0x02 && // SDO request
            EC_READ_U8 (data + 2) >> 5 == 0x04) { // Abort SDO transf. req.
            EC_ERR("SDO list download aborted on slave %i.\n",
                   slave->ring_position);
            ec_canopen_abort_msg(EC_READ_U32(data + 12));
            return -1;
        }

        if (EC_READ_U16(data) >> 12 == 0x08 && // SDO information
            (EC_READ_U8 (data + 2) & 0x7F) == 0x07) { // Get OD List response
            EC_ERR("SDO information error response at slave %i!\n",
                   slave->ring_position);
            ec_canopen_abort_msg(EC_READ_U32(data + 6));
            return -1;
        }

        if (EC_READ_U16(data) >> 12 != 0x08 || // SDO information
            (EC_READ_U8 (data + 2) & 0x7F) != 0x02) { // Get OD List response
            EC_ERR("Invalid SDO list response at slave %i!\n",
                   slave->ring_position);
            return -1;
        }

        if (rec_size < 8) {
            EC_ERR("Invalid data size!\n");
            return -1;
        }

#if 0
        for (i = 0; i < (rec_size - 8) / 2; i++)
            EC_INFO("Object 0x%04X\n", EC_READ_U16(data + 8 + i * 2));
#endif
    }
    while (EC_READ_U8(data + 2) & 0x80);

    return 0;
}

/*****************************************************************************/

/**
   Gibt eine SDO-Abort-Meldung aus.
*/

void ec_canopen_abort_msg(uint32_t abort_code)
{
    const ec_sdo_abort_message_t *abort_msg;

    for (abort_msg = sdo_abort_messages; abort_msg->code; abort_msg++) {
        if (abort_msg->code == abort_code) {
            EC_ERR("SDO abort message 0x%08X: \"%s\".\n",
                   abort_msg->code, abort_msg->message);
            return;
        }
    }
    EC_ERR("Unknown SDO abort code 0x%08X.\n", abort_code);
}

/*****************************************************************************/

const ec_sdo_abort_message_t sdo_abort_messages[] = {
    {0x05030000, "Toggle bit not changed"},
    {0x05040000, "SDO protocol timeout"},
    {0x05040001, "Client/Server command specifier not valid or unknown"},
    {0x05040005, "Out of memory"},
    {0x06010000, "Unsupported access to an object"},
    {0x06010001, "Attempt to read a write-only object"},
    {0x06010002, "Attempt to write a read-only object"},
    {0x06020000, "This object does not exist in the object directory"},
    {0x06040041, "The object cannot be mapped into the PDO"},
    {0x06040042, "The number and length of the objects to be mapped would"
     " exceed the PDO length"},
    {0x06040043, "General parameter incompatibility reason"},
    {0x06040047, "Gerneral internal incompatibility in device"},
    {0x06060000, "Access failure due to a hardware error"},
    {0x06070010, "Data type does not match, length of service parameter does"
     " not match"},
    {0x06070012, "Data type does not match, length of service parameter too"
     " high"},
    {0x06070013, "Data type does not match, length of service parameter too"
     " low"},
    {0x06090011, "Subindex does not exist"},
    {0x06090030, "Value range of parameter exceeded"},
    {0x06090031, "Value of parameter written too high"},
    {0x06090032, "Value of parameter written too low"},
    {0x06090036, "Maximum value is less than minimum value"},
    {0x08000000, "General error"},
    {0x08000020, "Data cannot be transferred or stored to the application"},
    {0x08000021, "Data cannot be transferred or stored to the application"
     " because of local control"},
    {0x08000022, "Data cannot be transferred or stored to the application"
     " because of the present device state"},
    {0x08000023, "Object dictionary dynamic generation fails or no object"
     " dictionary is present"},
    {}
};

/*****************************************************************************/

EXPORT_SYMBOL(ecrt_slave_sdo_write);
EXPORT_SYMBOL(ecrt_slave_sdo_read);
EXPORT_SYMBOL(ecrt_master_sdo_write);
EXPORT_SYMBOL(ecrt_master_sdo_read);

/*****************************************************************************/

/* Emacs-Konfiguration
;;; Local Variables: ***
;;; c-basic-offset:4 ***
;;; End: ***
*/
