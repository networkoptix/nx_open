#include "client_camera_factory.h"

#include <core/resource/client_storage_resource.h>
#include <core/resource/client_camera.h>

#include <nx/vms/api/data/media_server_data.h>

QnResourcePtr QnClientResourceFactory::createResource(const QnUuid &resourceTypeId,
    const QnResourceParams &)
{
    if (resourceTypeId == nx::vms::api::StorageData::kResourceTypeId)
    {
        QnClientStorageResourcePtr result(new QnClientStorageResource(nullptr));
        result->setActive(true);
        return result;
    }

    return QnResourcePtr(new QnClientCameraResource(resourceTypeId));
}

