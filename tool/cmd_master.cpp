/*****************************************************************************
 *
 * $Id$
 *
 ****************************************************************************/

#include <iostream>
#include <iomanip>
using namespace std;

#include "globals.h"

/****************************************************************************/

const char *help_master =
    "[OPTIONS]\n"
    "\n"
    "Show master and Ethernet device information.\n"
    "\n"
    "Command-specific options:\n"
    "  --master -m <index>  Index of the master to use. Default: 0.\n"
    "\n"
    "Numerical values can be specified either with decimal (no prefix),\n"
    "octal (prefix '0') or hexadecimal (prefix '0x') base.\n";

/****************************************************************************/

void command_master(void)
{
    ec_ioctl_master_t data;
    stringstream err;
    unsigned int i;
    
    masterDev.open(MasterDevice::Read);
    masterDev.getMaster(&data);

    cout
        << "Master" << masterIndex << endl
        << "  Phase: ";

    switch (data.phase) {
        case 0:  cout << "Waiting for device..."; break;
        case 1:  cout << "Idle"; break;
        case 2:  cout << "Operation"; break;
        default: cout << "???";
    }

    cout << endl
        << "  Slaves: " << data.slave_count << endl;

    for (i = 0; i < 2; i++) {
        cout << "  Device" << i << ": ";
        if (data.devices[i].address[0] == 0x00
                && data.devices[i].address[1] == 0x00
                && data.devices[i].address[2] == 0x00
                && data.devices[i].address[3] == 0x00
                && data.devices[i].address[4] == 0x00
                && data.devices[i].address[5] == 0x00) {
            cout << "None.";
        } else {
            cout << hex << setfill('0')
                << setw(2) << (unsigned int) data.devices[i].address[0] << ":"
                << setw(2) << (unsigned int) data.devices[i].address[1] << ":"
                << setw(2) << (unsigned int) data.devices[i].address[2] << ":"
                << setw(2) << (unsigned int) data.devices[i].address[3] << ":"
                << setw(2) << (unsigned int) data.devices[i].address[4] << ":"
                << setw(2) << (unsigned int) data.devices[i].address[5] << " ("
                << (data.devices[i].attached ? "attached" : "waiting...")
                << ")" << endl << dec
                << "    Tx count: " << data.devices[i].tx_count << endl
                << "    Rx count: " << data.devices[i].rx_count;
        }
        cout << endl;
    }
}

/*****************************************************************************/
