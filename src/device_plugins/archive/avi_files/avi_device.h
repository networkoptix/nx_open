#ifndef avi_device_h_1827
#define avi_device_h_1827

#include "..\abstract_archive_device.h"


class CLAviDevice : public CLAbstractArchiveDevice
{
public:
	CLAviDevice(const QString& file);
	~CLAviDevice();

	virtual CLStreamreader* getDeviceStreamConnection();
};


#endif //avi_device_h_1827