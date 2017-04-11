#include "client_camera_factory.h"

#include <core/resource/resource_type.h>
#include <core/resource/client_storage_resource.h>
#include <core/resource/client_camera.h>

QnResourcePtr QnClientResourceFactory::createResource(const QnUuid &resourceTypeId,
    const QnResourceParams &)
{
    if (resourceTypeId == qnResTypePool->getFixedResourceTypeId(QnResourceTypePool::kStorageTypeId))
    {
        QnClientStorageResourcePtr result(new QnClientStorageResource(nullptr));
        result->setActive(true);
        return result;
    }

    return QnResourcePtr(new QnClientCameraResource(resourceTypeId));
}

