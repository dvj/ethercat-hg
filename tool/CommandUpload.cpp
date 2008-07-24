/*****************************************************************************
 *
 * $Id$
 *
 ****************************************************************************/

#include <iostream>
#include <iomanip>
using namespace std;

#include "CommandUpload.h"
#include "coe_datatypes.h"
#include "byteorder.h"

/*****************************************************************************/

CommandUpload::CommandUpload():
    Command("upload", "Read an Sdo entry from a slave.")
{
}

/*****************************************************************************/

string CommandUpload::helpString() const
{
    stringstream str;

    str << getName() << " [OPTIONS] <INDEX> <SUBINDEX>" << endl
        << endl
        << getBriefDescription() << endl
        << endl
        << "The data type of the Sdo entry is taken from the Sdo" << endl
        << "dictionary by default. It can be overridden with the" << endl
        << "--type option. If the slave does not support the Sdo" << endl
        << "information service or the Sdo is not in the dictionary," << endl
        << "the --type option is mandatory."  << endl
        << endl
        << "These are the valid Sdo entry data types:" << endl
        << "  int8, int16, int32, uint8, uint16, uint32, string." << endl
        << endl
        << "Arguments:" << endl
        << "  INDEX    is the Sdo index and must be an unsigned" << endl
        << "           16 bit number." << endl
        << "  SUBINDEX is the Sdo entry subindex and must be an" << endl
        << "           unsigned 8 bit number." << endl
        << endl
        << "Command-specific options:" << endl
        << "  --slave -s <index>  Positive numerical ring position" << endl
        << "                      (mandatory)." << endl
        << "  --type  -t <type>   Forced Sdo entry data type (see" << endl
        << "                      above)." << endl
        << endl
        << numericInfo();

    return str.str();
}

/****************************************************************************/

void CommandUpload::execute(MasterDevice &m, const StringVector &args)
{
    stringstream err, strIndex, strSubIndex;
    int sval;
    ec_ioctl_slave_sdo_upload_t data;
    unsigned int uval;
    const CoEDataType *dataType = NULL;

    if (slavePosition < 0) {
        err << "'" << getName() << "' requires a slave! "
            << "Please specify --slave.";
        throwInvalidUsageException(err);
    }
    data.slave_position = slavePosition;

    if (args.size() != 2) {
        err << "'" << getName() << "' takes two arguments!";
        throwInvalidUsageException(err);
    }

    strIndex << args[0];
    strIndex
        >> resetiosflags(ios::basefield) // guess base from prefix
        >> data.sdo_index;
    if (strIndex.fail()) {
        err << "Invalid Sdo index '" << args[0] << "'!";
        throwInvalidUsageException(err);
    }

    strSubIndex << args[1];
    strSubIndex
        >> resetiosflags(ios::basefield) // guess base from prefix
        >> uval;
    if (strSubIndex.fail() || uval > 0xff) {
        err << "Invalid Sdo subindex '" << args[1] << "'!";
        throwInvalidUsageException(err);
    }
    data.sdo_entry_subindex = uval;

    if (dataTypeStr != "") { // data type specified
        if (!(dataType = findDataType(dataTypeStr))) {
            err << "Invalid data type '" << dataTypeStr << "'!";
            throwInvalidUsageException(err);
        }
    } else { // no data type specified: fetch from dictionary
        ec_ioctl_slave_sdo_entry_t entry;

        m.open(MasterDevice::Read);

        try {
            m.getSdoEntry(&entry, slavePosition,
                    data.sdo_index, data.sdo_entry_subindex);
        } catch (MasterDeviceException &e) {
            err << "Failed to determine Sdo entry data type. "
                << "Please specify --type.";
            throwCommandException(err);
        }
        if (!(dataType = findDataType(entry.data_type))) {
            err << "Pdo entry has unknown data type 0x"
                << hex << setfill('0') << setw(4) << entry.data_type << "!"
                << " Please specify --type.";
            throwCommandException(err);
        }
    }

    if (dataType->byteSize) {
        data.target_size = dataType->byteSize;
    } else {
        data.target_size = DefaultBufferSize;
    }

    data.target = new uint8_t[data.target_size + 1];

    m.open(MasterDevice::Read);

	try {
		m.sdoUpload(&data);
	} catch (MasterDeviceException &e) {
        delete [] data.target;
        throw e;
    }

    m.close();

    if (dataType->byteSize && data.data_size != dataType->byteSize) {
        err << "Data type mismatch. Expected " << dataType->name
            << " with " << dataType->byteSize << " byte, but got "
            << data.data_size << " byte.";
        throwCommandException(err);
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
            printRawData(data.target, data.data_size); // FIXME
            break;
    }

    delete [] data.target;
}

/****************************************************************************/

void CommandUpload::printRawData(
		const uint8_t *data,
		unsigned int size
		)
{
    cout << hex << setfill('0');
    while (size--) {
        cout << "0x" << setw(2) << (unsigned int) *data++;
        if (size)
            cout << " ";
    }
    cout << endl;
}

/*****************************************************************************/
