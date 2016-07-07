#include "client_camera_factory.h"

#include <core/resource/resource_type.h>
#include <core/resource/client_storage_resource.h>
#include <core/resource/client_camera.h>

QnResourcePtr QnClientResourceFactory::createResource(const QnUuid &resourceTypeId, const QnResourceParams &) {
    QnResourceTypePtr resourceType = qnResTypePool->getResourceType(resourceTypeId);

    if (resourceType.isNull())
        return QnResourcePtr();

    if (resourceTypeId == qnResTypePool->getFixedResourceTypeId(QnResourceTypePool::kStorageTypeId))
    {
        QnClientStorageResourcePtr result(new QnClientStorageResource());
        result->setActive(true);
        return result;
    }
    else
    {
        /* Currently we support only cameras. */
        if (!resourceType->isCamera())
            return QnResourcePtr();

        return QnResourcePtr(new QnClientCameraResource(resourceTypeId));
    }
}

