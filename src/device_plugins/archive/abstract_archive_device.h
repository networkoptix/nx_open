#ifndef abstract_archive_device_h1838
#define abstract_archive_device_h1838

#include "device/device.h"

class CLAbstractArchiveDevice : public CLDevice
{
public:
	CLAbstractArchiveDevice();
	~CLAbstractArchiveDevice();
	virtual DeviceType getDeviceType() const;

};

#endif //abstract_archive_device_h1838
