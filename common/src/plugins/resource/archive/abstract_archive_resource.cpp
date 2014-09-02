#include "abstract_archive_resource.h"

#ifdef ENABLE_ARCHIVE

QnAbstractArchiveResource::QnAbstractArchiveResource()
{
    QnMediaResource::initMediaResource();

    addFlags(Qn::ARCHIVE);
    setStatus(Qn::Online, true);
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


void QnAbstractArchiveResource::setStatus(Qn::ResourceStatus newStatus, bool silenceMode)
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

void QnAbstractArchiveResource::updateInner(const QnResourcePtr &other, QSet<QByteArray>& modifiedFields)
{
    QnResource::updateInner(other, modifiedFields);
    QnMediaResource::updateInner(other, modifiedFields);
}

#endif // ENABLE_ARCHIVE
