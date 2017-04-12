#include "webpage_resource.h"

QnWebPageResource::QnWebPageResource(QnCommonModule* commonModule):
    base_type(commonModule)
{
    setTypeId(qnResTypePool->getFixedResourceTypeId(QnResourceTypePool::kWebPageTypeId));
    addFlags(Qn::web_page);
}

QnWebPageResource::QnWebPageResource(const QUrl& url, QnCommonModule* commonModule):
    QnWebPageResource(commonModule)
{
    setId(QnUuid::createUuid());
    setName(nameForUrl(url));
    setUrl(url.toString());
}

QnWebPageResource::~QnWebPageResource()
{
}

void QnWebPageResource::setUrl(const QString& url)
{
    base_type::setUrl(url);
    setStatus(Qn::Online);
}

QString QnWebPageResource::nameForUrl(const QUrl& url)
{
    QString name = url.host();
    if (!url.path().isEmpty())
        name += L'/' + url.path();
    return name;
}

Qn::ResourceStatus QnWebPageResource::getStatus() const
{
    QnMutexLocker lock(&m_mutex);
    return m_status;
}

void QnWebPageResource::setStatus(Qn::ResourceStatus newStatus, Qn::StatusChangeReason reason)
{
    {
        QnMutexLocker lock(&m_mutex);
        if (m_status == newStatus)
            return;
        m_status = newStatus;
    }
    emit statusChanged(toSharedPointer(), reason);
}
