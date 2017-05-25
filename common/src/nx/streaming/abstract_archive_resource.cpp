#include "abstract_archive_resource.h"

QnAbstractArchiveResource::QnAbstractArchiveResource()
{
    QnMediaResource::initMediaResource();

    addFlags(Qn::local_video);
    m_localStatus = Qn::Online;
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

Qn::ResourceStatus QnAbstractArchiveResource::getStatus() const
{
    return m_localStatus;
}

void QnAbstractArchiveResource::setStatus(Qn::ResourceStatus newStatus, Qn::StatusChangeReason reason)
{
    Q_UNUSED(reason)
    m_localStatus = newStatus;
}
