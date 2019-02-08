#include "abstract_archive_resource.h"
#include <nx/utils/log/log.h>

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
    if (m_localStatus == newStatus)
        return;

    m_localStatus = newStatus;

    // Null pointer if we are changing status in constructor. Signal is not needed in this case.
    if (auto sharedThis = toSharedPointer(this))
    {
        NX_VERBOSE(this, "Signal status change for %1", newStatus);
        emit statusChanged(sharedThis, reason);
    }
}
