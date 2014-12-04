#include "abstract_archive_resource.h"

#ifdef ENABLE_ARCHIVE

QnAbstractArchiveResource::QnAbstractArchiveResource()
{
    QnMediaResource::initMediaResource();

    addFlags(Qn::ARCHIVE);
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

void QnAbstractArchiveResource::updateInner(const QnResourcePtr &other, QSet<QByteArray>& modifiedFields)
{
    QnResource::updateInner(other, modifiedFields);
    QnMediaResource::updateInner(other, modifiedFields);
}

Qn::ResourceStatus QnAbstractArchiveResource::getStatus() const
{
    return m_localStatus;
}

void QnAbstractArchiveResource::setStatus(Qn::ResourceStatus newStatus, bool silenceMode)
{
    m_localStatus = newStatus;
}


#endif // ENABLE_ARCHIVE
