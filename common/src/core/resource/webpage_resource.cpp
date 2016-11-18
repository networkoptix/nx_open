#include "webpage_resource.h"

QnWebPageResource::QnWebPageResource()
    : base_type()
{
    setTypeId(qnResTypePool->getFixedResourceTypeId(QnResourceTypePool::kWebPageTypeId));
    addFlags(Qn::web_page);
}

QnWebPageResource::QnWebPageResource(const QUrl& url)
    : QnWebPageResource()
{
    setId(guidFromArbitraryData(url.toString().toUtf8()));
    setName(url.host());
    setUrl(url.toString());
}

QnWebPageResource::~QnWebPageResource()
{
}

QString QnWebPageResource::getUniqueId() const
{
    return getUrl();
}

void QnWebPageResource::setUrl(const QString& url)
{
    base_type::setUrl(url);
    setStatus(Qn::Online);
}
