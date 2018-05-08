#include "c2p_webpage_searcher.h"

#include <core/resource/webpage_resource.h>
#include <core/resource/resource_data.h>
#include <core/resource_management/resource_data_pool.h>
#include <common/common_module.h>
#include <common/static_common_module.h>

namespace
{
static const QString kC2pScheme("c2p");
}

QnPlC2pResourceSearcher::QnPlC2pResourceSearcher(QnCommonModule* commonModule):
    QnAbstractResourceSearcher(commonModule),
    QnAbstractNetworkResourceSearcher(commonModule)
{
}

QnResourcePtr QnPlC2pResourceSearcher::createResource(
    const QnUuid &resourceTypeId, const QnResourceParams& /*params*/)
{
    return QnC2pWebPageResourcePtr();
}

QnResourceList QnPlC2pResourceSearcher::findResources()
{
    return QnResourceList();
}

QString QnPlC2pResourceSearcher::manufacture() const
{
    return QnResourceTypePool::kC2pWebPageTypeId;
}

QList<QnResourcePtr> QnPlC2pResourceSearcher::checkHostAddr(
    const QUrl& url, const QAuthenticator& auth, bool isSearchAction)
{
    QList<QnResourcePtr> result;
    if (url.scheme().toLower() == kC2pScheme && url.isValid())
    {
        QnC2pWebPageResourcePtr resource(new QnC2pWebPageResource(url.url(), commonModule()));
        QnUuid resourceTypeId =
            qnResTypePool->getResourceTypeId(QnResourceTypePool::kC2pWebPageTypeId, "");
        resource->setTypeId(resourceTypeId);

        result.push_back(resource);
    }
    return result;
}
