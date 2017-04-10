#include "mobile_client_camera_factory.h"

#include <core/resource/resource_type.h>
#include <core/resource/client_storage_resource.h>

#include "mobile_client_camera.h"
#include <client_core/client_core_module.h>

QnResourcePtr QnMobileClientCameraFactory::createResource(const QnUuid &resourceTypeId, const QnResourceParams &)
{
    auto storageTypeId = qnResTypePool->getFixedResourceTypeId(QnResourceTypePool::kStorageTypeId);
    if (storageTypeId == resourceTypeId)
        return QnResourcePtr(new QnClientStorageResource(qnClientCoreModule->commonModule()));

    /* Currently we support only cameras. */
    QnResourceTypePtr resourceType = qnResTypePool->getResourceType(resourceTypeId);
    if (!resourceType.isNull() && !resourceType->isCamera())
        return QnResourcePtr();

    return QnResourcePtr(new QnMobileClientCamera(resourceTypeId));
}
