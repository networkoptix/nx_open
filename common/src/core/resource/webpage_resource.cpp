#include "webpage_resource.h"

QnWebPageResource::QnWebPageResource()
    : base_type()
{
    setTypeId(qnResTypePool->getFixedResourceTypeId(QnResourceTypePool::kWebPageTypeId));
}

QnWebPageResource::QnWebPageResource(const QUrl &url)
    : base_type()
{
    setId(guidFromArbitraryData(url.toString().toUtf8()));
    setTypeId(qnResTypePool->getFixedResourceTypeId(QnResourceTypePool::kWebPageTypeId));
    setUrl(url.toString());
    setName(url.host());
}

QnWebPageResource::~QnWebPageResource()
{
}

QString QnWebPageResource::getUniqueId() const
{
    return getUrl();
}
