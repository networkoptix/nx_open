#include "abstract_archive_resource.h"


QnAbstractArchiveResource::QnAbstractArchiveResource()
{
	addDeviceTypeFlag(QnResource::ARCHIVE);
}

QnAbstractArchiveResource::~QnAbstractArchiveResource()
{

}

QnResource::DeviceType QnAbstractArchiveResource::getDeviceType() const
{
	return VIDEODEVICE;
}
