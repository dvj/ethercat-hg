/*****************************************************************************
 *
 * $Id$
 *
 ****************************************************************************/

#ifndef __MASTER_DEVICE_H__
#define __MASTER_DEVICE_H__

#include <stdexcept>
#include <sstream>
using namespace std;

#include "../include/ecrt.h"
#include "../master/ioctl.h"

/****************************************************************************/

class MasterDeviceException:
    public runtime_error
{
    friend class MasterDevice;
    
    protected:
        /** Constructor with stringstream parameter. */
        MasterDeviceException(
                const stringstream &s /**< Message. */
                ): runtime_error(s.str()) {}
};

/****************************************************************************/

class MasterDevice
{
    public:
        MasterDevice();
        ~MasterDevice();

        void setIndex(unsigned int);
		unsigned int getIndex() const;

        enum Permissions {Read, ReadWrite};
        void open(Permissions);
        void close();

        void getMaster(ec_ioctl_master_t *);
        void getConfig(ec_ioctl_config_t *, unsigned int);
        void getConfigPdo(ec_ioctl_config_pdo_t *, unsigned int, uint8_t,
                uint16_t);
        void getConfigPdoEntry(ec_ioctl_config_pdo_entry_t *, unsigned int,
                uint8_t, uint16_t, uint8_t);
        void getConfigSdo(ec_ioctl_config_sdo_t *, unsigned int, unsigned int);
        void getDomain(ec_ioctl_domain_t *, unsigned int);
        void getFmmu(ec_ioctl_domain_fmmu_t *, unsigned int, unsigned int);
        void getData(ec_ioctl_domain_data_t *, unsigned int, unsigned int,
                unsigned char *);
        void getSlave(ec_ioctl_slave_t *, uint16_t);
        void getSync(ec_ioctl_slave_sync_t *, uint16_t, uint8_t);
        void getPdo(ec_ioctl_slave_sync_pdo_t *, uint16_t, uint8_t, uint8_t);
        void getPdoEntry(ec_ioctl_slave_sync_pdo_entry_t *, uint16_t, uint8_t,
                uint8_t, uint8_t);
        void getSdo(ec_ioctl_slave_sdo_t *, uint16_t, uint16_t);
        void getSdoEntry(ec_ioctl_slave_sdo_entry_t *, uint16_t, int, uint8_t);
        void readSii(ec_ioctl_slave_sii_t *);
        void writeSii(ec_ioctl_slave_sii_t *);
		void setDebug(unsigned int);
		void sdoDownload(ec_ioctl_slave_sdo_download_t *);
		void sdoUpload(ec_ioctl_slave_sdo_upload_t *);
		void requestState(uint16_t, uint8_t);

    private:
        unsigned int index;
        int fd;
};

/****************************************************************************/

inline unsigned int MasterDevice::getIndex() const
{
	return index;
}

/****************************************************************************/

#endif
