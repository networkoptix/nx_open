#include "abstract_archive_resource.h"


QnAbstractArchiveResource::QnAbstractArchiveResource()
{
	addFlags(QnResource::ARCHIVE);
    setStatus(Online, true);
}

QnAbstractArchiveResource::~QnAbstractArchiveResource()
{

}

QString QnAbstractArchiveResource::getUniqueId() const
{
    return getUrl();
}


void QnAbstractArchiveResource::setStatus(QnResource::Status /*newStatus*/, bool silenceMode)
{
    QnResource::setStatus(Online, silenceMode);
    return;
}