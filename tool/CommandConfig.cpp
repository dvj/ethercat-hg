/*****************************************************************************
 *
 *  $Id$
 *
 *  Copyright (C) 2006-2009  Florian Pose, Ingenieurgemeinschaft IgH
 *
 *  This file is part of the IgH EtherCAT Master.
 *
 *  The IgH EtherCAT Master is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License version 2, as
 *  published by the Free Software Foundation.
 *
 *  The IgH EtherCAT Master is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General
 *  Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with the IgH EtherCAT Master; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 *  ---
 *
 *  The license mentioned above concerns the source code only. Using the
 *  EtherCAT technology and brand is only permitted in compliance with the
 *  industrial property and similar rights of Beckhoff Automation GmbH.
 *
 ****************************************************************************/

#include <list>
#include <iostream>
#include <iomanip>
#include <sstream>
using namespace std;

#include "CommandConfig.h"

/*****************************************************************************/

CommandConfig::CommandConfig():
    Command("config", "Show slave configurations.")
{
}

/*****************************************************************************/

string CommandConfig::helpString() const
{
    stringstream str;

    str << getName() << " [OPTIONS]" << endl
    	<< endl
    	<< getBriefDescription() << endl
    	<< endl
    	<< "Without the --verbose option, slave configurations are" << endl
    	<< "output one-per-line. Example:" << endl
    	<< endl
    	<< "1001:0  0x0000003b/0x02010000  3  OP" << endl
    	<< "|       |                      |  |" << endl
    	<< "|       |                      |  \\- Application-layer" << endl
    	<< "|       |                      |     state of the attached" << endl
    	<< "|       |                      |     slave, or '-', if no" << endl
    	<< "|       |                      |     slave is attached." << endl
    	<< "|       |                      \\- Absolute decimal ring" << endl
    	<< "|       |                         position of the attached" << endl
    	<< "|       |                         slave, or '-' if none" << endl
    	<< "|       |                         attached." << endl
    	<< "|       \\- Expected vendor ID and product code (both" << endl
    	<< "|          hexadecimal)." << endl
    	<< "\\- Alias address and relative position (both decimal)." << endl
    	<< endl
    	<< "With the --verbose option given, the configured PDOs and" << endl
    	<< "SDOs are output in addition." << endl
        << endl
        << "Configuration selection:" << endl
        << "  Slave configurations can be selected with" << endl
        << "  the --alias and --position parameters as follows:" << endl
        << endl
        << "  1) If neither the --alias nor the --position option" << endl
        << "     is given, all slave configurations are displayed." << endl
        << "  2) If only the --position option is given, an alias" << endl
        << "     of zero is assumed (see 4))." << endl
        << "  3) If only the --alias option is given, all slave" << endl
        << "     configurations with the given alias address" << endl
        << "     are displayed." << endl
        << "  4) If both the --alias and the --position option are" << endl
        << "     given, the selection can match a single" << endl
        << "     configuration, that is displayed, if it exists." << endl
    	<< endl
    	<< "Command-specific options:" << endl
        << "  --alias    -a <alias>  Configuration alias (see above)." << endl
        << "  --position -p <pos>    Relative position (see above)." << endl
    	<< "  --verbose  -v          Show detailed configurations." << endl
        << endl
        << numericInfo();

	return str.str();
}

/*****************************************************************************/

/** Lists the bus configuration.
 */
void CommandConfig::execute(MasterDevice &m, const StringVector &args)
{
    ConfigList configs;

    if (args.size()) {
        stringstream err;
        err << "'" << getName() << "' takes no arguments!";
        throwInvalidUsageException(err);
    }

    m.open(MasterDevice::Read);
    configs = selectedConfigs(m);

    if (getVerbosity() == Verbose) {
        showDetailedConfigs(m, configs);
    } else {
        listConfigs(m, configs);
    }
}

/*****************************************************************************/

/** Lists the complete bus configuration.
 */
void CommandConfig::showDetailedConfigs(
		MasterDevice &m,
		const ConfigList &configList
		)
{
    ConfigList::const_iterator configIter;
    unsigned int j, k, l;
    ec_ioctl_slave_t slave;
    ec_ioctl_config_pdo_t pdo;
    ec_ioctl_config_pdo_entry_t entry;
    ec_ioctl_config_sdo_t sdo;

    for (configIter = configList.begin();
            configIter != configList.end();
            configIter++) {

        cout << "Alias: "
            << dec << configIter->alias << endl
            << "Position: " << configIter->position << endl
            << "Vendor Id: 0x"
            << hex << setfill('0')
            << setw(8) << configIter->vendor_id << endl
            << "Product code: 0x"
            << setw(8) << configIter->product_code << endl
            << "Attached slave: ";
        
        if (configIter->slave_position != -1) {
            m.getSlave(&slave, configIter->slave_position);
            cout << dec << configIter->slave_position
                << " (" << alStateString(slave.al_state) << ")" << endl;
        } else {
            cout << "none" << endl;
        }

        for (j = 0; j < EC_MAX_SYNC_MANAGERS; j++) {
            if (configIter->syncs[j].pdo_count) {
                cout << "SM" << dec << j << " ("
                    << (configIter->syncs[j].dir == EC_DIR_INPUT
                            ? "Input" : "Output") << ")" << endl;
                for (k = 0; k < configIter->syncs[j].pdo_count; k++) {
                    m.getConfigPdo(&pdo, configIter->config_index, j, k);

                    cout << "  PDO 0x" << hex << setfill('0')
                        << setw(4) << pdo.index << endl;

                    for (l = 0; l < pdo.entry_count; l++) {
                        m.getConfigPdoEntry(&entry,
                                configIter->config_index, j, k, l);

                        cout << "    PDO entry 0x" << hex << setfill('0')
                            << setw(4) << entry.index << ":"
                            << setw(2) << (unsigned int) entry.subindex
                            << ", " << dec << setfill(' ')
                            << setw(2) << (unsigned int) entry.bit_length
                            << " bit" << endl;
                    }
                }
            }
        }

        cout << "SDO configuration:" << endl;
        if (configIter->sdo_count) {
            for (j = 0; j < configIter->sdo_count; j++) {
                m.getConfigSdo(&sdo, configIter->config_index, j);

                cout << "  0x"
                    << hex << setfill('0')
                    << setw(4) << sdo.index << ":"
                    << setw(2) << (unsigned int) sdo.subindex
                    << ", " << dec << sdo.size << " byte: " << hex;

                switch (sdo.size) {
                    case 1:
                        cout << "0x" << setw(2)
                            << (unsigned int) *(uint8_t *) &sdo.data;
                        break;
                    case 2:
                        cout << "0x" << setw(4)
                            << le16_to_cpup(&sdo.data);
                        break;
                    case 4:
                        cout << "0x" << setw(8)
                            << le32_to_cpup(&sdo.data);
                        break;
                    default:
                        cout << "???";
                }

                cout << endl;
            }
        } else {
            cout << "  None." << endl;
        }

        if (configIter->dc_assign_activate) {
            int i;

            cout << "DC configuration:" << endl
                << "  AssignActivate: 0x" << hex << setfill('0')
                << setw(4) << configIter->dc_assign_activate << endl;

            cout << "         Cycle [ns]   Shift [ns]" << endl;
            for (i = 0; i < EC_SYNC_SIGNAL_COUNT; i++) {
                cout << "  SYNC" << dec << i << "  "
                    << setfill(' ') << right
                    << setw(11) << configIter->dc_sync[i].cycle_time
                    << "  "
                    << setw(11) << configIter->dc_sync[i].shift_time
                    << endl;
            }
        }
        cout << endl;
    }
}

/*****************************************************************************/

/** Lists the bus configuration.
 */
void CommandConfig::listConfigs(
        MasterDevice &m,
        const ConfigList &configList
        )
{
    ConfigList::const_iterator configIter;
    stringstream str;
    Info info;
    typedef list<Info> InfoList;
    InfoList list;
    InfoList::const_iterator iter;
    unsigned int maxAliasWidth = 0, maxPosWidth = 0,
                 maxSlavePosWidth = 0, maxStateWidth = 0;
    ec_ioctl_slave_t slave;

    for (configIter = configList.begin();
            configIter != configList.end();
            configIter++) {

        str << dec << configIter->alias;
        info.alias = str.str();
        str.clear();
        str.str("");

        str << configIter->position;
        info.pos = str.str();
        str.clear();
        str.str("");

        str << hex << setfill('0')
            << "0x" << setw(8) << configIter->vendor_id
            << "/0x" << setw(8) << configIter->product_code;
        info.ident = str.str();
        str.clear();
        str.str("");

        if (configIter->slave_position != -1) {
            m.getSlave(&slave, configIter->slave_position);

            str << dec << configIter->slave_position;
            info.slavePos = str.str();
            str.clear();
            str.str("");

            str << alStateString(slave.al_state);
            info.state = str.str();
            str.clear();
            str.str("");
        } else {
            str << "-";
            info.slavePos = str.str();
            str.clear();
            str.str("");

            str << "-";
            info.state = str.str();
            str.clear();
            str.str("");
        }

        list.push_back(info);

        if (info.alias.length() > maxAliasWidth)
            maxAliasWidth = info.alias.length();
        if (info.pos.length() > maxPosWidth)
            maxPosWidth = info.pos.length();
        if (info.slavePos.length() > maxSlavePosWidth)
            maxSlavePosWidth = info.slavePos.length();
        if (info.state.length() > maxStateWidth)
            maxStateWidth = info.state.length();
    }

    for (iter = list.begin(); iter != list.end(); iter++) {
        cout << setfill(' ') << right
            << setw(maxAliasWidth) << iter->alias
            << ":" << left
            << setw(maxPosWidth) << iter->pos
            << "  "
            << iter->ident
            << "  "
            << setw(maxSlavePosWidth) << iter->slavePos << "  "
            << setw(maxStateWidth) << iter->state << "  "
            << endl;
    }
}

/*****************************************************************************/
