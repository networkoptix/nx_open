#include "abstract_archive_device.h"


CLAbstractArchiveDevice::CLAbstractArchiveDevice()
{
	addDeviceTypeFlag(CLDevice::ARCHIVE);
}

CLAbstractArchiveDevice::~CLAbstractArchiveDevice()
{

}

CLDevice::DeviceType CLAbstractArchiveDevice::getDeviceType() const
{
	return VIDEODEVICE;
}
