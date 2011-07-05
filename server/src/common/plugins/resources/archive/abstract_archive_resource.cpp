#include "abstract_archive_resource.h"


CLAbstractArchiveDevice::CLAbstractArchiveDevice()
{
	addDeviceTypeFlag(QnResource::ARCHIVE);
}

CLAbstractArchiveDevice::~CLAbstractArchiveDevice()
{

}

QnResource::DeviceType CLAbstractArchiveDevice::getDeviceType() const
{
	return VIDEODEVICE;
}
