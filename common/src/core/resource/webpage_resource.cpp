#include "webpage_resource.h"

QnWebPageResource::QnWebPageResource()
    : base_type()
{
    setTypeId(qnResTypePool->getFixedResourceTypeId(QnResourceTypePool::kWebPageTypeId));
    addFlags(Qn::web_page);
}

QnWebPageResource::QnWebPageResource(const QUrl &url)
    : base_type()
{
    setId(guidFromArbitraryData(url.toString().toUtf8()));
    setTypeId(qnResTypePool->getFixedResourceTypeId(QnResourceTypePool::kWebPageTypeId));
    setUrl(url.toString());
    setName(url.host());
    setStatus(Qn::Online);
    addFlags(Qn::web_page);
}

QnWebPageResource::~QnWebPageResource()
{
}

QString QnWebPageResource::getUniqueId() const
{
    return getUrl();
}
