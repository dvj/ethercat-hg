/*****************************************************************************
 *
 * $Id$
 *
 ****************************************************************************/

#include <iostream>
#include <iomanip>
using namespace std;

#include "globals.h"
#include "coe_datatypes.h"

/****************************************************************************/

// FIXME
const char *help_upload =
    "[OPTIONS]\n"
    "\n"
    "\n"
    "Command-specific options:\n";

/****************************************************************************/

void command_upload(void)
{
    stringstream strIndex, strSubIndex;
    int sval;
    ec_ioctl_slave_sdo_upload_t data;
    unsigned int uval;
    const CoEDataType *dataType = NULL;

    if (slavePosition < 0) {
        stringstream err;
        err << "'sdo_upload' requires a slave! Please specify --slave.";
        throw MasterDeviceException(err.str());
    }
    data.slave_position = slavePosition;

    if (commandArgs.size() != 2) {
        stringstream err;
        err << "'sdo_upload' takes two arguments!";
        throw MasterDeviceException(err.str());
    }

    strIndex << commandArgs[0];
    strIndex
        >> resetiosflags(ios::basefield) // guess base from prefix
        >> data.sdo_index;
    if (strIndex.fail()) {
        stringstream err;
        err << "Invalid Sdo index '" << commandArgs[0] << "'!";
        throw MasterDeviceException(err.str());
    }

    strSubIndex << commandArgs[1];
    strSubIndex
        >> resetiosflags(ios::basefield) // guess base from prefix
        >> uval;
    if (strSubIndex.fail() || uval > 0xff) {
        stringstream err;
        err << "Invalid Sdo subindex '" << commandArgs[1] << "'!";
        throw MasterDeviceException(err.str());
    }
    data.sdo_entry_subindex = uval;

    if (dataTypeStr != "") { // data type specified
        if (!(dataType = findDataType(dataTypeStr))) {
            stringstream err;
            err << "Invalid data type '" << dataTypeStr << "'!";
            throw MasterDeviceException(err.str());
        }
    } else { // no data type specified: fetch from dictionary
        ec_ioctl_slave_sdo_entry_t entry;

        masterDev.open(MasterDevice::Read);

        try {
            masterDev.getSdoEntry(&entry, slavePosition,
                    data.sdo_index, data.sdo_entry_subindex);
        } catch (MasterDeviceException &e) {
            stringstream err;
            err << "Failed to determine Sdo entry data type. "
                << "Please specify --type.";
            throw ExecutionFailureException(err);
        }
        if (!(dataType = findDataType(entry.data_type))) {
            stringstream err;
            err << "Pdo entry has unknown data type 0x"
                << hex << setfill('0') << setw(4) << entry.data_type << "!"
                << " Please specify --type.";
            throw ExecutionFailureException(err);
        }
    }

    if (dataType->byteSize) {
        data.target_size = dataType->byteSize;
    } else {
        data.target_size = DefaultBufferSize;
    }

    data.target = new uint8_t[data.target_size + 1];

    masterDev.open(MasterDevice::Read);

	try {
		masterDev.sdoUpload(&data);
	} catch (MasterDeviceException &e) {
        delete [] data.target;
        throw e;
    }

    masterDev.close();

    if (dataType->byteSize && data.data_size != dataType->byteSize) {
        stringstream err;
        err << "Data type mismatch. Expected " << dataType->name
            << " with " << dataType->byteSize << " byte, but got "
            << data.data_size << " byte.";
        throw MasterDeviceException(err.str());
    }

    cout << setfill('0');
    switch (dataType->coeCode) {
        case 0x0002: // int8
            sval = *(int8_t *) data.target;
            cout << sval << " 0x" << hex << setw(2) << sval << endl;
            break;
        case 0x0003: // int16
            sval = le16tocpu(*(int16_t *) data.target);
            cout << sval << " 0x" << hex << setw(4) << sval << endl;
            break;
        case 0x0004: // int32
            sval = le32tocpu(*(int32_t *) data.target);
            cout << sval << " 0x" << hex << setw(8) << sval << endl;
            break;
        case 0x0005: // uint8
            uval = (unsigned int) *(uint8_t *) data.target;
            cout << uval << " 0x" << hex << setw(2) << uval << endl;
            break;
        case 0x0006: // uint16
            uval = le16tocpu(*(uint16_t *) data.target);
            cout << uval << " 0x" << hex << setw(4) << uval << endl;
            break;
        case 0x0007: // uint32
            uval = le32tocpu(*(uint32_t *) data.target);
            cout << uval << " 0x" << hex << setw(8) << uval << endl;
            break;
        case 0x0009: // string
            cout << string((const char *) data.target, data.data_size)
                << endl;
            break;
        default:
            printRawData(data.target, data.data_size);
            break;
    }

    delete [] data.target;
}

/*****************************************************************************/
