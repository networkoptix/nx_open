#include "abstract_archive_resource.h"


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
