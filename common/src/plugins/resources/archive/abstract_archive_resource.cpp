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

void QnAbstractArchiveResource::setUniqId(const QString& value)
{
    setUrl(value);
}


void QnAbstractArchiveResource::setStatus(QnResource::Status newStatus, bool silenceMode)
{
    QnResource::setStatus(newStatus, silenceMode);
    return;
}

const QnResource* QnAbstractArchiveResource::toResource() const
{
    return this;
}

QnResource* QnAbstractArchiveResource::toResource()
{
    return this;
}

const QnResourcePtr QnAbstractArchiveResource::toResourcePtr() const
{
    return toSharedPointer();
}

QnResourcePtr QnAbstractArchiveResource::toResourcePtr()
{
    return toSharedPointer();
}
