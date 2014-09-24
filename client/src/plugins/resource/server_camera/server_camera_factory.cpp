#include "server_camera_factory.h"

#include <core/resource/resource_type.h>
#include <core/resource/abstract_storage_resource.h>

#include "server_camera.h"

QnResourcePtr QnServerCameraFactory::createResource(const QUuid &resourceTypeId, const QnResourceParams &) {
    QnResourceTypePtr resourceType = qnResTypePool->getResourceType(resourceTypeId);

    if (resourceType.isNull())
        return QnResourcePtr();

    if (resourceType->getName() == QLatin1String("Storage")) {
        return QnResourcePtr(new QnAbstractStorageResource());
    } else {
        /* Currently we support only cameras. */
        if (!resourceType->isCamera())
            return QnResourcePtr();

        return QnResourcePtr(new QnServerCamera(resourceTypeId));
    }
}

