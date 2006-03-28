/******************************************************************************
 *
 *  s l a v e . c
 *
 *  Methoden f�r einen EtherCAT-Slave.
 *
 *  $Id$
 *
 *****************************************************************************/

#include <linux/module.h>
#include <linux/delay.h>

#include "globals.h"
#include "slave.h"
#include "command.h"
#include "master.h"

/*****************************************************************************/

int ec_slave_fetch_categories(ec_slave_t *);
int ec_slave_fetch_strings(ec_slave_t *, const uint8_t *);
int ec_slave_fetch_general(ec_slave_t *, const uint8_t *);
int ec_slave_fetch_sync(ec_slave_t *, const uint8_t *, size_t);
int ec_slave_fetch_pdo(ec_slave_t *, const uint8_t *, size_t, ec_pdo_type_t);
int ec_slave_locate_string(ec_slave_t *, unsigned int, char **);

/*****************************************************************************/

/**
   EtherCAT-Slave-Konstruktor.
*/

void ec_slave_init(ec_slave_t *slave, /**< EtherCAT-Slave */
                   ec_master_t *master /**< EtherCAT-Master */
                   )
{
    slave->master = master;
    slave->base_type = 0;
    slave->base_revision = 0;
    slave->base_build = 0;
    slave->base_fmmu_count = 0;
    slave->base_sync_count = 0;
    slave->ring_position = 0;
    slave->station_address = 0;
    slave->sii_alias = 0;
    slave->sii_vendor_id = 0;
    slave->sii_product_code = 0;
    slave->sii_revision_number = 0;
    slave->sii_serial_number = 0;
    slave->sii_rx_mailbox_offset = 0;
    slave->sii_rx_mailbox_size = 0;
    slave->sii_tx_mailbox_offset = 0;
    slave->sii_tx_mailbox_size = 0;
    slave->sii_mailbox_protocols = 0;
    slave->type = NULL;
    slave->registered = 0;
    slave->fmmu_count = 0;
    slave->eeprom_name = NULL;
    slave->eeprom_group = NULL;
    slave->eeprom_desc = NULL;
    INIT_LIST_HEAD(&slave->eeprom_strings);
    INIT_LIST_HEAD(&slave->eeprom_syncs);
    INIT_LIST_HEAD(&slave->eeprom_pdos);
    INIT_LIST_HEAD(&slave->sdo_dictionary);
}

/*****************************************************************************/

/**
   EtherCAT-Slave-Destruktor.
*/

void ec_slave_clear(ec_slave_t *slave /**< EtherCAT-Slave */)
{
    ec_eeprom_string_t *string, *next_str;
    ec_eeprom_sync_t *sync, *next_sync;
    ec_eeprom_pdo_t *pdo, *next_pdo;
    ec_eeprom_pdo_entry_t *entry, *next_ent;
    ec_sdo_t *sdo, *next_sdo;

    // Alle Strings freigeben
    list_for_each_entry_safe(string, next_str, &slave->eeprom_strings, list) {
        list_del(&string->list);
        kfree(string);
    }

    // Alle Sync-Manager freigeben
    list_for_each_entry_safe(sync, next_sync, &slave->eeprom_syncs, list) {
        list_del(&sync->list);
        kfree(sync);
    }

    // Alle PDOs freigeben
    list_for_each_entry_safe(pdo, next_pdo, &slave->eeprom_pdos, list) {
        list_del(&pdo->list);
        if (pdo->name) kfree(pdo->name);

        // Alle Entries innerhalb eines PDOs freigeben
        list_for_each_entry_safe(entry, next_ent, &pdo->entries, list) {
            list_del(&entry->list);
            if (entry->name) kfree(entry->name);
            kfree(entry);
        }

        kfree(pdo);
    }

    if (slave->eeprom_name) kfree(slave->eeprom_name);
    if (slave->eeprom_group) kfree(slave->eeprom_group);
    if (slave->eeprom_desc) kfree(slave->eeprom_desc);

    // Alle SDOs freigeben
    list_for_each_entry_safe(sdo, next_sdo, &slave->sdo_dictionary, list) {
        list_del(&sdo->list);
        if (sdo->name) kfree(sdo->name);
        kfree(sdo);
    }
}

/*****************************************************************************/

/**
   Liest alle ben�tigten Informationen aus einem Slave.

   \return 0 wenn alles ok, < 0 bei Fehler.
*/

int ec_slave_fetch(ec_slave_t *slave /**< EtherCAT-Slave */)
{
    ec_command_t command;

    // Read base data
    ec_command_init_nprd(&command, slave->station_address, 0x0000, 6);
    if (unlikely(ec_master_simple_io(slave->master, &command))) {
        EC_ERR("Reading base datafrom slave %i failed!\n",
               slave->ring_position);
        return -1;
    }

    slave->base_type =       EC_READ_U8 (command.data);
    slave->base_revision =   EC_READ_U8 (command.data + 1);
    slave->base_build =      EC_READ_U16(command.data + 2);
    slave->base_fmmu_count = EC_READ_U8 (command.data + 4);
    slave->base_sync_count = EC_READ_U8 (command.data + 5);

    if (slave->base_fmmu_count > EC_MAX_FMMUS)
        slave->base_fmmu_count = EC_MAX_FMMUS;

    if (ec_slave_sii_read16(slave, 0x0004, &slave->sii_alias))
        return -1;
    if (ec_slave_sii_read32(slave, 0x0008, &slave->sii_vendor_id))
        return -1;
    if (ec_slave_sii_read32(slave, 0x000A, &slave->sii_product_code))
        return -1;
    if (ec_slave_sii_read32(slave, 0x000C, &slave->sii_revision_number))
        return -1;
    if (ec_slave_sii_read32(slave, 0x000E, &slave->sii_serial_number))
        return -1;
    if (ec_slave_sii_read16(slave, 0x0018, &slave->sii_rx_mailbox_offset))
        return -1;
    if (ec_slave_sii_read16(slave, 0x0019, &slave->sii_rx_mailbox_size))
        return -1;
    if (ec_slave_sii_read16(slave, 0x001A, &slave->sii_tx_mailbox_offset))
        return -1;
    if (ec_slave_sii_read16(slave, 0x001B, &slave->sii_tx_mailbox_size))
        return -1;
    if (ec_slave_sii_read16(slave, 0x001C, &slave->sii_mailbox_protocols))
        return -1;

    if (unlikely(ec_slave_fetch_categories(slave))) {
        EC_ERR("Failed to fetch category data!\n");
        return -1;
    }

    return 0;
}

/*****************************************************************************/

/**
   Liest 16 Bit aus dem Slave-Information-Interface
   eines EtherCAT-Slaves.

   \return 0 bei Erfolg, sonst < 0
*/

int ec_slave_sii_read16(ec_slave_t *slave,
                        /**< EtherCAT-Slave */
                        uint16_t offset,
                        /**< Adresse des zu lesenden SII-Registers */
                        uint16_t *target
                        /**< Speicher f�r Wert (16-Bit) */
                        )
{
    ec_command_t command;
    uint8_t data[10];
    cycles_t start, end, timeout;

    // Initiate read operation

    EC_WRITE_U8 (data,     0x00); // read-only access
    EC_WRITE_U8 (data + 1, 0x01); // request read operation
    EC_WRITE_U32(data + 2, offset);

    ec_command_init_npwr(&command, slave->station_address, 0x502, 6, data);
    if (unlikely(ec_master_simple_io(slave->master, &command))) {
        EC_ERR("SII-read failed on slave %i!\n", slave->ring_position);
        return -1;
    }

    // Der Slave legt die Informationen des Slave-Information-Interface
    // in das Datenregister und l�scht daraufhin ein Busy-Bit. Solange
    // den Status auslesen, bis das Bit weg ist.

    start = get_cycles();
    timeout = (cycles_t) 100 * cpu_khz; // 100ms

    while (1)
    {
        udelay(10);

        ec_command_init_nprd(&command, slave->station_address, 0x502, 10);
        if (unlikely(ec_master_simple_io(slave->master, &command))) {
            EC_ERR("Getting SII-read status failed on slave %i!\n",
                   slave->ring_position);
            return -1;
        }

        end = get_cycles();

        if (likely((EC_READ_U8(command.data + 1) & 0x81) == 0)) {
            *target = EC_READ_U16(command.data + 6);
            return 0;
        }

        if (unlikely((end - start) >= timeout)) {
            EC_ERR("SII-read. Slave %i timed out!\n", slave->ring_position);
            return -1;
        }
    }
}

/*****************************************************************************/

/**
   Liest 32 Bit aus dem Slave-Information-Interface
   eines EtherCAT-Slaves.

   \return 0 bei Erfolg, sonst < 0
*/

int ec_slave_sii_read32(ec_slave_t *slave,
                        /**< EtherCAT-Slave */
                        uint16_t offset,
                        /**< Adresse des zu lesenden SII-Registers */
                        uint32_t *target
                        /**< Speicher f�r Wert (32-Bit) */
                        )
{
    ec_command_t command;
    uint8_t data[10];
    cycles_t start, end, timeout;

    // Initiate read operation

    EC_WRITE_U8 (data,     0x00); // read-only access
    EC_WRITE_U8 (data + 1, 0x01); // request read operation
    EC_WRITE_U32(data + 2, offset);

    ec_command_init_npwr(&command, slave->station_address, 0x502, 6, data);
    if (unlikely(ec_master_simple_io(slave->master, &command))) {
        EC_ERR("SII-read failed on slave %i!\n", slave->ring_position);
        return -1;
    }

    // Der Slave legt die Informationen des Slave-Information-Interface
    // in das Datenregister und l�scht daraufhin ein Busy-Bit. Solange
    // den Status auslesen, bis das Bit weg ist.

    start = get_cycles();
    timeout = (cycles_t) 100 * cpu_khz; // 100ms

    while (1)
    {
        udelay(10);

        ec_command_init_nprd(&command, slave->station_address, 0x502, 10);
        if (unlikely(ec_master_simple_io(slave->master, &command))) {
            EC_ERR("Getting SII-read status failed on slave %i!\n",
                   slave->ring_position);
            return -1;
        }

        end = get_cycles();

        if (likely((EC_READ_U8(command.data + 1) & 0x81) == 0)) {
            *target = EC_READ_U32(command.data + 6);
            return 0;
        }

        if (unlikely((end - start) >= timeout)) {
            EC_ERR("SII-read. Slave %i timed out!\n", slave->ring_position);
            return -1;
        }
    }
}

/*****************************************************************************/

/**
   Schreibt 16 Bit Daten in das Slave-Information-Interface
   eines EtherCAT-Slaves.

   \return 0 bei Erfolg, sonst < 0
*/

int ec_slave_sii_write16(ec_slave_t *slave,
                         /**< EtherCAT-Slave */
                         uint16_t offset,
                         /**< Adresse des zu lesenden SII-Registers */
                         uint16_t value
                         /**< Zu schreibender Wert */
                         )
{
    ec_command_t command;
    uint8_t data[8];
    cycles_t start, end, timeout;

    EC_INFO("SII-write (slave %i, offset 0x%04X, value 0x%04X)\n",
            slave->ring_position, offset, value);

    // Initiate write operation

    EC_WRITE_U8 (data,     0x01); // enable write access
    EC_WRITE_U8 (data + 1, 0x02); // request write operation
    EC_WRITE_U32(data + 2, offset);
    EC_WRITE_U16(data + 6, value);

    ec_command_init_npwr(&command, slave->station_address, 0x502, 8, data);
    if (unlikely(ec_master_simple_io(slave->master, &command))) {
        EC_ERR("SII-write failed on slave %i!\n", slave->ring_position);
        return -1;
    }

    // Der Slave legt die Informationen des Slave-Information-Interface
    // in das Datenregister und l�scht daraufhin ein Busy-Bit. Solange
    // den Status auslesen, bis das Bit weg ist.

    start = get_cycles();
    timeout = (cycles_t) 100 * cpu_khz; // 100ms

    while (1)
    {
        udelay(10);

        ec_command_init_nprd(&command, slave->station_address, 0x502, 2);
        if (unlikely(ec_master_simple_io(slave->master, &command))) {
            EC_ERR("Getting SII-write status failed on slave %i!\n",
                   slave->ring_position);
            return -1;
        }

        end = get_cycles();

        if (likely((EC_READ_U8(command.data + 1) & 0x82) == 0)) {
            if (EC_READ_U8(command.data + 1) & 0x40) {
                EC_ERR("SII-write failed!\n");
                return -1;
            }
            else {
                EC_INFO("SII-write succeeded!\n");
                return 0;
            }
        }

        if (unlikely((end - start) >= timeout)) {
            EC_ERR("SII-write: Slave %i timed out!\n", slave->ring_position);
            return -1;
        }
    }
}

/*****************************************************************************/

/**
   Holt Daten aus dem EEPROM.

   \return 0, wenn alles ok, sonst < 0
*/

int ec_slave_fetch_categories(ec_slave_t *slave /**< EtherCAT-Slave */)
{
    uint16_t word_offset, cat_type, word_count;
    uint32_t value;
    uint8_t *cat_data;
    unsigned int i;

    word_offset = 0x0040;

    if (!(cat_data = (uint8_t *) kmalloc(0x10000, GFP_KERNEL))) {
        EC_ERR("Failed to allocate 64k bytes for category data.\n");
        return -1;
    }

    while (1) {
        // read category type
        if (ec_slave_sii_read32(slave, word_offset, &value)) {
            EC_ERR("Unable to read category header.\n");
            goto out_free;
        }

        // Last category?
        if ((value & 0xFFFF) == 0xFFFF) break;

        cat_type = value & 0x7FFF;
        word_count = (value >> 16) & 0xFFFF;

        // Fetch category data
        for (i = 0; i < word_count; i++) {
            if (ec_slave_sii_read32(slave, word_offset + 2 + i, &value)) {
                EC_ERR("Unable to read category data word %i.\n", i);
                goto out_free;
            }

            cat_data[i * 2]     = (value >> 0) & 0xFF;
            cat_data[i * 2 + 1] = (value >> 8) & 0xFF;

            // read second word "on the fly"
            if (i + 1 < word_count) {
                i++;
                cat_data[i * 2]     = (value >> 16) & 0xFF;
                cat_data[i * 2 + 1] = (value >> 24) & 0xFF;
            }
        }

        switch (cat_type)
        {
            case 0x000A:
                if (ec_slave_fetch_strings(slave, cat_data))
                    goto out_free;
                break;
            case 0x001E:
                if (ec_slave_fetch_general(slave, cat_data))
                    goto out_free;
                break;
            case 0x0028:
                break;
            case 0x0029:
                if (ec_slave_fetch_sync(slave, cat_data, word_count))
                    goto out_free;
                break;
            case 0x0032:
                if (ec_slave_fetch_pdo(slave, cat_data, word_count, EC_TX_PDO))
                    goto out_free;
                break;
            case 0x0033:
                if (ec_slave_fetch_pdo(slave, cat_data, word_count, EC_RX_PDO))
                    goto out_free;
                break;
            default:
                EC_WARN("Unknown category type 0x%04X in slave %i.\n",
                        cat_type, slave->ring_position);
        }

        word_offset += 2 + word_count;
    }

    kfree(cat_data);
    return 0;

 out_free:
    kfree(cat_data);
    return -1;
}

/*****************************************************************************/

/**
   Holt die Daten einer String-Kategorie.

   \return 0 wenn alles ok, sonst < 0
*/

int ec_slave_fetch_strings(ec_slave_t *slave, /**< EtherCAT-Slave */
                           const uint8_t *data /**< Kategoriedaten */
                           )
{
    unsigned int string_count, i;
    size_t size;
    off_t offset;
    ec_eeprom_string_t *string;

    string_count = data[0];
    offset = 1;
    for (i = 0; i < string_count; i++) {
        size = data[offset];
        // Speicher f�r String-Objekt und Daten in einem Rutsch allozieren
        if (!(string = (ec_eeprom_string_t *)
              kmalloc(sizeof(ec_eeprom_string_t) + size + 1, GFP_KERNEL))) {
            EC_ERR("Failed to allocate string memory.\n");
            return -1;
        }
        string->size = size;
        string->data = (char *) string + sizeof(ec_eeprom_string_t);
        memcpy(string->data, data + offset + 1, size);
        string->data[size] = 0x00;
        list_add_tail(&string->list, &slave->eeprom_strings);
        offset += 1 + size;
    }

    return 0;
}

/*****************************************************************************/

/**
   Holt die Daten einer General-Kategorie.
*/

int ec_slave_fetch_general(ec_slave_t *slave, /**< EtherCAT-Slave */
                           const uint8_t *data /**< Kategorie-Daten */
                           )
{
    if (ec_slave_locate_string(slave, data[0], &slave->eeprom_group))
        return -1;
    if (ec_slave_locate_string(slave, data[1], &slave->eeprom_name))
        return -1;
    if (ec_slave_locate_string(slave, data[3], &slave->eeprom_desc))
        return -1;

    return 0;
}

/*****************************************************************************/

/**
   Holt die Daten einer Sync-Manager-Kategorie.
*/

int ec_slave_fetch_sync(ec_slave_t *slave, /**< EtherCAT-Slave */
                        const uint8_t *data, /**< Kategorie-Daten */
                        size_t word_count /**< Anzahl Words */
                        )
{
    unsigned int sync_count, i;
    ec_eeprom_sync_t *sync;

    sync_count = word_count / 4; // Sync-Manager-Strunktur ist 4 Worte lang

    for (i = 0; i < sync_count; i++, data += 8) {
        if (!(sync = (ec_eeprom_sync_t *)
              kmalloc(sizeof(ec_eeprom_sync_t), GFP_KERNEL))) {
            EC_ERR("Failed to allocate Sync-Manager memory.\n");
            return -1;
        }

        sync->index = i;
        sync->physical_start_address = *((uint16_t *) (data + 0));
        sync->length                 = *((uint16_t *) (data + 2));
        sync->control_register       = data[4];
        sync->enable                 = data[6];

        list_add_tail(&sync->list, &slave->eeprom_syncs);
    }

    return 0;
}

/*****************************************************************************/

/**
   Holt die Daten einer TXPDO-Kategorie.
*/

int ec_slave_fetch_pdo(ec_slave_t *slave, /**< EtherCAT-Slave */
                       const uint8_t *data, /**< Kategorie-Daten */
                       size_t word_count, /**< Anzahl Worte */
                       ec_pdo_type_t pdo_type /**< PDO-Typ */
                       )
{
    ec_eeprom_pdo_t *pdo;
    ec_eeprom_pdo_entry_t *entry;
    unsigned int entry_count, i;

    while (word_count >= 4) {
        if (!(pdo = (ec_eeprom_pdo_t *)
              kmalloc(sizeof(ec_eeprom_pdo_t), GFP_KERNEL))) {
            EC_ERR("Failed to allocate PDO memory.\n");
            return -1;
        }

        INIT_LIST_HEAD(&pdo->entries);
        pdo->type = pdo_type;

        pdo->index = *((uint16_t *) data);
        entry_count = data[2];
        pdo->sync_manager = data[3];
        pdo->name = NULL;
        ec_slave_locate_string(slave, data[5], &pdo->name);

        list_add_tail(&pdo->list, &slave->eeprom_pdos);

        word_count -= 4;
        data += 8;

        for (i = 0; i < entry_count; i++) {
            if (!(entry = (ec_eeprom_pdo_entry_t *)
                  kmalloc(sizeof(ec_eeprom_pdo_entry_t), GFP_KERNEL))) {
                EC_ERR("Failed to allocate PDO entry memory.\n");
                return -1;
            }

            entry->index = *((uint16_t *) data);
            entry->subindex = data[2];
            entry->name = NULL;
            ec_slave_locate_string(slave, data[3], &entry->name);
            entry->bit_length = data[5];

            list_add_tail(&entry->list, &pdo->entries);

            word_count -= 4;
            data += 8;
        }
    }

    return 0;
}

/*****************************************************************************/

/**
   Durchsucht die tempor�ren Strings und dupliziert den gefundenen String.
*/

int ec_slave_locate_string(ec_slave_t *slave, unsigned int index, char **ptr)
{
    ec_eeprom_string_t *string;
    char *err_string;

    // Erst alten Speicher freigeben
    if (*ptr) {
        kfree(*ptr);
        *ptr = NULL;
    }

    // Index 0 bedeutet "nicht belegt"
    if (!index) return 0;

    // EEPROM-String mit Index finden und kopieren
    list_for_each_entry(string, &slave->eeprom_strings, list) {
        if (--index) continue;

        if (!(*ptr = (char *) kmalloc(string->size + 1, GFP_KERNEL))) {
            EC_ERR("Unable to allocate string memory.\n");
            return -1;
        }
        memcpy(*ptr, string->data, string->size + 1);
        return 0;
    }

    EC_WARN("String %i not found in slave %i.\n", index, slave->ring_position);

    err_string = "(string not found)";

    if (!(*ptr = (char *) kmalloc(strlen(err_string) + 1, GFP_KERNEL))) {
        EC_ERR("Unable to allocate string memory.\n");
        return -1;
    }

    memcpy(*ptr, err_string, strlen(err_string) + 1);
    return 0;
}

/*****************************************************************************/

/**
   Best�tigt einen Fehler beim Zustandswechsel.

   \todo Funktioniert noch nicht...
*/

void ec_slave_state_ack(ec_slave_t *slave,
                        /**< Slave, dessen Zustand ge�ndert werden soll */
                        uint8_t state
                        /**< Alter Zustand */
                        )
{
    ec_command_t command;
    uint8_t data[2];
    cycles_t start, end, timeout;

    EC_WRITE_U16(data, state | EC_ACK);

    ec_command_init_npwr(&command, slave->station_address, 0x0120, 2, data);
    if (unlikely(ec_master_simple_io(slave->master, &command))) {
        EC_WARN("State %02X acknowledge failed on slave %i!\n",
                state, slave->ring_position);
        return;
    }

    start = get_cycles();
    timeout = (cycles_t) 10 * cpu_khz; // 10ms

    while (1)
    {
        udelay(100); // Dem Slave etwas Zeit lassen...

        ec_command_init_nprd(&command, slave->station_address, 0x0130, 2);
        if (unlikely(ec_master_simple_io(slave->master, &command))) {
            EC_WARN("State %02X acknowledge checking failed on slave %i!\n",
                    state, slave->ring_position);
            return;
        }

        end = get_cycles();

        if (unlikely(EC_READ_U8(command.data) != state)) {
            EC_WARN("Could not acknowledge state %02X on slave %i (code"
                    " %02X)!\n", state, slave->ring_position,
                    EC_READ_U8(command.data));
            return;
        }

        if (likely(EC_READ_U8(command.data) == state)) {
            EC_INFO("Acknowleged state %02X on slave %i.\n", state,
                    slave->ring_position);
            return;
        }

        if (unlikely((end - start) >= timeout)) {
            EC_WARN("Could not check state acknowledgement %02X of slave %i -"
                    " Timeout while checking!\n", state, slave->ring_position);
            return;
        }
    }
}

/*****************************************************************************/

/**
   �ndert den Zustand eines Slaves.

   \return 0 bei Erfolg, sonst < 0
*/

int ec_slave_state_change(ec_slave_t *slave,
                          /**< Slave, dessen Zustand ge�ndert werden soll */
                          uint8_t state
                          /**< Neuer Zustand */
                          )
{
    ec_command_t command;
    uint8_t data[2];
    cycles_t start, end, timeout;

    EC_WRITE_U16(data, state);

    ec_command_init_npwr(&command, slave->station_address, 0x0120, 2, data);
    if (unlikely(ec_master_simple_io(slave->master, &command))) {
        EC_ERR("Failed to set state %02X on slave %i!\n",
               state, slave->ring_position);
        return -1;
    }

    start = get_cycles();
    timeout = (cycles_t) 10 * cpu_khz; // 10ms

    while (1)
    {
        udelay(100); // Dem Slave etwas Zeit lassen...

        ec_command_init_nprd(&command, slave->station_address, 0x0130, 2);
        if (unlikely(ec_master_simple_io(slave->master, &command))) {
            EC_ERR("Failed to check state %02X on slave %i!\n",
                   state, slave->ring_position);
            return -1;
        }

        end = get_cycles();

        if (unlikely(EC_READ_U8(command.data) & 0x10)) { // State change error
            EC_ERR("Could not set state %02X - Slave %i refused state change"
                   " (code %02X)!\n", state, slave->ring_position,
                   EC_READ_U8(command.data));
            ec_slave_state_ack(slave, EC_READ_U8(command.data) & 0x0F);
            return -1;
        }

        if (likely(EC_READ_U8(command.data) == (state & 0x0F))) {
            // State change successful
            return 0;
        }

        if (unlikely((end - start) >= timeout)) {
            EC_ERR("Could not check state %02X of slave %i - Timeout!\n",
                   state, slave->ring_position);
            return -1;
        }
    }
}

/*****************************************************************************/

/**
   Merkt eine FMMU-Konfiguration vor.

   Die FMMU wird so konfiguriert, dass sie den gesamten Datenbereich des
   entsprechenden Sync-Managers abdeckt. F�r jede Dom�ne werden separate
   FMMUs konfiguriert.

   Wenn die entsprechende FMMU bereits konfiguriert ist, wird dies als
   Erfolg zur�ckgegeben.

   \return 0 bei Erfolg, sonst < 0
*/

int ec_slave_set_fmmu(ec_slave_t *slave, /**< EtherCAT-Slave */
                      const ec_domain_t *domain, /**< Dom�ne */
                      const ec_sync_t *sync  /**< Sync-Manager */
                      )
{
    unsigned int i;

    // FMMU schon vorgemerkt?
    for (i = 0; i < slave->fmmu_count; i++)
        if (slave->fmmus[i].domain == domain && slave->fmmus[i].sync == sync)
            return 0;

    // Neue FMMU reservieren...

    if (slave->fmmu_count >= slave->base_fmmu_count) {
        EC_ERR("Slave %i FMMU limit reached!\n", slave->ring_position);
        return -1;
    }

    slave->fmmus[slave->fmmu_count].domain = domain;
    slave->fmmus[slave->fmmu_count].sync = sync;
    slave->fmmus[slave->fmmu_count].logical_start_address = 0;
    slave->fmmu_count++;
    slave->registered = 1;

    return 0;
}

/*****************************************************************************/

/**
   Gibt alle Informationen �ber einen EtherCAT-Slave aus.
*/

void ec_slave_print(const ec_slave_t *slave /**< EtherCAT-Slave */)
{
    ec_eeprom_sync_t *sync;
    ec_eeprom_pdo_t *pdo;
    ec_eeprom_pdo_entry_t *entry;
    ec_sdo_t *sdo;
    int first;

    EC_INFO("x-- EtherCAT slave information ---------------\n");

    if (slave->type) {
        EC_INFO("| Vendor \"%s\", Product \"%s\": %s\n",
                slave->type->vendor_name, slave->type->product_name,
                slave->type->description);
    }
    else {
        EC_INFO("| *** This slave has no type information! ***\n");
    }

    EC_INFO("| Ring position: %i, Station address: 0x%04X\n",
            slave->ring_position, slave->station_address);

    EC_INFO("| Base information:\n");
    EC_INFO("|   Type %u, Revision %i, Build %i\n",
            slave->base_type, slave->base_revision, slave->base_build);
    EC_INFO("|   Supported FMMUs: %i, Sync managers: %i\n",
            slave->base_fmmu_count, slave->base_sync_count);

    if (slave->sii_mailbox_protocols) {
        EC_INFO("| Mailbox communication:\n");
        EC_INFO("|   RX mailbox: 0x%04X/%i, TX mailbox: 0x%04X/%i\n",
                slave->sii_rx_mailbox_offset, slave->sii_rx_mailbox_size,
                slave->sii_tx_mailbox_offset, slave->sii_tx_mailbox_size);
        EC_INFO("|   Supported protocols: ");

        first = 1;
        if (slave->sii_mailbox_protocols & EC_MBOX_AOE) {
            printk("AoE");
            first = 0;
        }
        if (slave->sii_mailbox_protocols & EC_MBOX_EOE) {
            if (!first) printk(", ");
            printk("EoE");
            first = 0;
        }
        if (slave->sii_mailbox_protocols & EC_MBOX_COE) {
            if (!first) printk(", ");
            printk("CoE");
            first = 0;
        }
        if (slave->sii_mailbox_protocols & EC_MBOX_FOE) {
            if (!first) printk(", ");
            printk("FoE");
            first = 0;
        }
        if (slave->sii_mailbox_protocols & EC_MBOX_SOE) {
            if (!first) printk(", ");
            printk("SoE");
            first = 0;
        }
        if (slave->sii_mailbox_protocols & EC_MBOX_VOE) {
            if (!first) printk(", ");
            printk("VoE");
        }
        printk("\n");
    }

    EC_INFO("| EEPROM data:\n");

    if (slave->sii_alias)
        EC_INFO("|   Configured station alias: 0x%04X (%i)\n",
                slave->sii_alias, slave->sii_alias);

    EC_INFO("|   Vendor-ID: 0x%08X, Product code: 0x%08X\n",
            slave->sii_vendor_id, slave->sii_product_code);
    EC_INFO("|   Revision number: 0x%08X, Serial number: 0x%08X\n",
            slave->sii_revision_number, slave->sii_serial_number);

    if (slave->eeprom_name)
        EC_INFO("|   Name: %s\n", slave->eeprom_name);
    if (slave->eeprom_group)
        EC_INFO("|   Group: %s\n", slave->eeprom_group);
    if (slave->eeprom_desc)
        EC_INFO("|   Description: %s\n", slave->eeprom_desc);

    if (!list_empty(&slave->eeprom_syncs)) {
        EC_INFO("|   Sync-Managers:\n");
        list_for_each_entry(sync, &slave->eeprom_syncs, list) {
            EC_INFO("|     %i: 0x%04X, length %i, control 0x%02X, %s\n",
                    sync->index, sync->physical_start_address, sync->length,
                    sync->control_register,
                    sync->enable ? "enable" : "disable");
        }
    }

    list_for_each_entry(pdo, &slave->eeprom_pdos, list) {
        EC_INFO("|   %s \"%s\" (0x%04X), -> Sync-Manager %i\n",
                pdo->type == EC_RX_PDO ? "RXPDO" : "TXPDO",
                pdo->name ? pdo->name : "???",
                pdo->index, pdo->sync_manager);

        list_for_each_entry(entry, &pdo->entries, list) {
            EC_INFO("|     \"%s\" 0x%04X:%X, %i Bit\n",
                    entry->name ? entry->name : "???",
                    entry->index, entry->subindex, entry->bit_length);
        }
    }

    if (!list_empty(&slave->sdo_dictionary)) {
        EC_INFO("|   SDO-Dictionary:\n");
        list_for_each_entry(sdo, &slave->sdo_dictionary, list) {
            EC_INFO("|     0x%04X: \"%s\"\n", sdo->index,
                    sdo->name ? sdo->name : "");
            EC_INFO("|       Type 0x%04X, subindices: %i, features: 0x%02X\n",
                    sdo->type, sdo->max_subindex, sdo->features);
        }
    }

    EC_INFO("x---------------------------------------------\n");
}

/*****************************************************************************/

/**
   Gibt die Z�hlerst�nde der CRC-Fault-Counter aus und setzt diese zur�ck.

   \return 0 bei Erfolg, sonst < 0
*/

int ec_slave_check_crc(ec_slave_t *slave /**< EtherCAT-Slave */)
{
    ec_command_t command;
    uint8_t data[4];

    ec_command_init_nprd(&command, slave->station_address, 0x0300, 4);
    if (unlikely(ec_master_simple_io(slave->master, &command))) {
        EC_WARN("Reading CRC fault counters failed on slave %i!\n",
                slave->ring_position);
        return -1;
    }

    // No CRC faults.
    if (!EC_READ_U32(command.data)) return 0;

    if (EC_READ_U8(command.data))
        EC_WARN("%3i RX-error%s on slave %i, channel A.\n",
                EC_READ_U8(command.data),
                EC_READ_U8(command.data) == 1 ? "" : "s",
                slave->ring_position);
    if (EC_READ_U8(command.data + 1))
        EC_WARN("%3i invalid frame%s on slave %i, channel A.\n",
                EC_READ_U8(command.data + 1),
                EC_READ_U8(command.data + 1) == 1 ? "" : "s",
                slave->ring_position);
    if (EC_READ_U8(command.data + 2))
        EC_WARN("%3i RX-error%s on slave %i, channel B.\n",
                EC_READ_U8(command.data + 2),
                EC_READ_U8(command.data + 2) == 1 ? "" : "s",
                slave->ring_position);
    if (EC_READ_U8(command.data + 3))
        EC_WARN("%3i invalid frame%s on slave %i, channel B.\n",
                EC_READ_U8(command.data + 3),
                EC_READ_U8(command.data + 3) == 1 ? "" : "s",
                slave->ring_position);

    // Reset CRC counters
    EC_WRITE_U32(data, 0x00000000);
    ec_command_init_npwr(&command, slave->station_address, 0x0300, 4, data);
    if (unlikely(ec_master_simple_io(slave->master, &command))) {
        EC_WARN("Resetting CRC fault counters failed on slave %i!\n",
                slave->ring_position);
        return -1;
    }

    return 0;
}

/*****************************************************************************/

/**
   Sendet ein Mailbox-Kommando.
 */

int ec_slave_mailbox_send(ec_slave_t *slave, /**< EtherCAT-Slave */
                          uint8_t type, /**< Unterliegendes Protokoll */
                          const uint8_t *prot_data, /**< Protokoll-Daten */
                          size_t size /**< Datengr��e */
                          )
{
    size_t total_size;
    uint8_t *data;
    ec_command_t command;

    if (unlikely(!slave->sii_mailbox_protocols)) {
        EC_ERR("Slave %i does not support mailbox communication!\n",
               slave->ring_position);
        return -1;
    }

    total_size = size + 6;
    if (unlikely(total_size > slave->sii_rx_mailbox_size)) {
        EC_ERR("Data size does not fit in mailbox!\n");
        return -1;
    }

    if (!(data = kmalloc(slave->sii_rx_mailbox_size, GFP_KERNEL))) {
        EC_ERR("Failed to allocate %i bytes of memory for mailbox data!\n",
               slave->sii_rx_mailbox_size);
        return -1;
    }

    memset(data, 0x00, slave->sii_rx_mailbox_size);
    EC_WRITE_U16(data,      size); // Length of the Mailbox service data
    EC_WRITE_U16(data + 2,  slave->station_address); // Station address
    EC_WRITE_U8 (data + 4,  0x00); // Channel & priority
    EC_WRITE_U8 (data + 5,  type); // Underlying protocol type
    memcpy(data + 6, prot_data, size);

    ec_command_init_npwr(&command, slave->station_address,
                         slave->sii_rx_mailbox_offset,
                         slave->sii_rx_mailbox_size, data);
    if (unlikely(ec_master_simple_io(slave->master, &command))) {
        EC_ERR("Mailbox sending failed on slave %i!\n", slave->ring_position);
        kfree(data);
        return -1;
    }

    kfree(data);
    return 0;
}

/*****************************************************************************/

/**
   Sendet ein Mailbox-Kommando.
 */

int ec_slave_mailbox_receive(ec_slave_t *slave, /**< EtherCAT-Slave */
                             uint8_t type, /**< Unterliegendes Protokoll */
                             uint8_t *prot_data, /**< Protokoll-Daten */
                             size_t *size /**< Datengr��e des Puffers, sp�ter
                                             Gr��e der gelesenen Daten */
                             )
{
    ec_command_t command;
    size_t data_size;
    cycles_t start, end, timeout;

    // Read "written bit" of Sync-Manager
    start = get_cycles();
    timeout = (cycles_t) 100 * cpu_khz; // 100ms

    while (1)
    {
        // FIXME: Zweiter Sync-Manager nicht immer TX-Mailbox?
        ec_command_init_nprd(&command, slave->station_address, 0x808, 8);
        if (unlikely(ec_master_simple_io(slave->master, &command))) {
            EC_ERR("Mailbox checking failed on slave %i!\n",
                   slave->ring_position);
            return -1;
        }

        end = get_cycles();

        if (EC_READ_U8(command.data + 5) & 8)
            break; // Proceed with received data

        if ((end - start) >= timeout) {
            EC_ERR("Mailbox check - Slave %i timed out.\n",
                   slave->ring_position);
            return -1;
        }

        udelay(100);
    }

    ec_command_init_nprd(&command, slave->station_address,
                         slave->sii_tx_mailbox_offset,
                         slave->sii_tx_mailbox_size);
    if (unlikely(ec_master_simple_io(slave->master, &command))) {
        EC_ERR("Mailbox receiving failed on slave %i!\n",
               slave->ring_position);
        return -1;
    }

    if ((EC_READ_U8(command.data + 5) & 0x0F) != type) {
        EC_ERR("Unexpected mailbox protocol 0x%02X (exp.: 0x%02X) at"
               " slave %i!\n", EC_READ_U8(command.data + 5), type,
               slave->ring_position);
        return -1;
    }

    if (unlikely(slave->master->debug_level) > 1)
        EC_DBG("Mailbox receive took %ius.\n", ((u32) (end - start) * 1000
                                                / cpu_khz));

    if ((data_size = EC_READ_U16(command.data)) > *size) {
        EC_ERR("Mailbox service data does not fit into buffer (%i > %i).\n",
               data_size, *size);
        return -1;
    }

    if (data_size > slave->sii_tx_mailbox_size - 6) {
        EC_ERR("Currupt mailbox response detected!\n");
        return -1;
    }

    memcpy(prot_data, command.data + 6, data_size);
    *size = data_size;
    return 0;
}

/*****************************************************************************/

/* Emacs-Konfiguration
;;; Local Variables: ***
;;; c-basic-offset:4 ***
;;; End: ***
*/

