#include "abstract_archive_device.h"

QnAbstractArchiveResource::QnAbstractArchiveResource()
{
	addFlag(QnResource::ARCHIVE);
}

QnAbstractArchiveResource::~QnAbstractArchiveResource()
{

}

QString QnAbstractArchiveResource::getUniqueId() const
{
    return getUrl();
}
