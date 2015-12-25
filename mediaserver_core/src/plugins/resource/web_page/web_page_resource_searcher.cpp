#include "web_page_resource_searcher.h"

#include <core/resource/webpage_resource.h>
#include <core/resource/resource_factory.h>

namespace
{
    bool isValidUrl(const QUrl &url) {
        const static QString kHttp(lit("http"));
        const static QString kHttps(lit("https"));

        return url.scheme() == kHttp || url.scheme() == kHttps;
    }
}

QnWebPageResourceSearcher::QnWebPageResourceSearcher()
{
}

QList<QnResourcePtr> QnWebPageResourceSearcher::checkHostAddr(const QUrl& url, const QAuthenticator& auth, bool doMultichannelCheck)
{
    QN_UNUSED(auth, doMultichannelCheck);

    QList<QnResourcePtr> result;
    if (isValidUrl(url))
        result << QnWebPageResourcePtr(new QnWebPageResource(url));
    return result;
}

QnResourcePtr QnWebPageResourceSearcher::createResource(const QnUuid &resourceTypeId, const QnResourceParams &params)
{
    QUrl url(params.url);
    if (!isValidUrl(url))
        return QnResourcePtr();
    return QnWebPageResourcePtr(new QnWebPageResource(url));
}

QString QnWebPageResourceSearcher::manufacture() const
{
    return QnResourceTypePool::kWebPageTypeId;
}

QnResourceList QnWebPageResourceSearcher::findResources()
{
    return QnResourceList();
}

bool QnWebPageResourceSearcher::isResourceTypeSupported(QnUuid resourceTypeId) const
{
    return resourceTypeId == qnResTypePool->getFixedResourceTypeId(QnResourceTypePool::kWebPageTypeId);
}
