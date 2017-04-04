#include "webpage_resource.h"

QnWebPageResource::QnWebPageResource():
    base_type()
{
    setTypeId(qnResTypePool->getFixedResourceTypeId(QnResourceTypePool::kWebPageTypeId));
    addFlags(Qn::web_page);
}

QnWebPageResource::QnWebPageResource(const QUrl& url):
    QnWebPageResource()
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
