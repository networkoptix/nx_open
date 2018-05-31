#include "c2p_camera_resource_searcher.h"

#include <core/resource/security_cam_resource.h>
#include <core/resource/resource_data.h>
#include <core/resource_management/resource_data_pool.h>
#include <common/common_module.h>
#include <common/static_common_module.h>

namespace
{
static const QString kC2pScheme("c2p");
static const QString kManufacture("C2P");
}

QnPlC2pCameraResourceSearcher::QnPlC2pCameraResourceSearcher(QnCommonModule* commonModule):
    QnAbstractResourceSearcher(commonModule),
    QnAbstractNetworkResourceSearcher(commonModule)
{
}

QnResourcePtr QnPlC2pCameraResourceSearcher::createResource(
    const QnUuid &resourceTypeId, const QnResourceParams& /*params*/)
{
    return QnC2pCameraResourcePtr();
}

QnResourceList QnPlC2pCameraResourceSearcher::findResources()
{
    return QnResourceList();
}

QString QnPlC2pCameraResourceSearcher::manufacture() const
{
    return QnResourceTypePool::kC2pCameraTypeId;
}

QList<QnResourcePtr> QnPlC2pCameraResourceSearcher::checkHostAddr(
    const nx::utils::Url& url, const QAuthenticator& auth, bool isSearchAction)
{
    QList<QnResourcePtr> result;
    if (url.scheme().toLower() == kC2pScheme && url.isValid())
    {
        QnC2pCameraResourcePtr resource(new QnC2pCameraResource(commonModule()));
        QnUuid resourceTypeId = qnResTypePool->getResourceTypeId(
            kManufacture /*manufacture*/,
            QnResourceTypePool::kC2pCameraTypeId /*name*/);
        resource->setTypeId(resourceTypeId);
        resource->setUrl(url.toString());
        resource->setAuth(auth);

        result.push_back(resource);
    }
    return result;
}
