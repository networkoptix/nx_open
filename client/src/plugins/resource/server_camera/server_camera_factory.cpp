#include "server_camera_factory.h"

#include <core/resource/resource_type.h>
#include <core/resource/client_storage_resource.h>

#include "server_camera.h"

QnResourcePtr QnServerCameraFactory::createResource(const QnUuid &resourceTypeId, const QnResourceParams &) {
    QnResourceTypePtr resourceType = qnResTypePool->getResourceType(resourceTypeId);

    if (resourceType.isNull())
        return QnResourcePtr();

    if (resourceType->getName() == QLatin1String("Storage")) {
        return QnResourcePtr(new QnClientStorageResource());
    } else {
        /* Currently we support only cameras. */
        if (!resourceType->isCamera())
            return QnResourcePtr();

        return QnResourcePtr(new QnServerCamera(resourceTypeId));
    }
}

