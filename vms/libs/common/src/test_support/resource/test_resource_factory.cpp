#include "test_resource_factory.h"

#include <test_support/resource/storage_resource_stub.h>
#include <test_support/resource/camera_resource_stub.h>

#include <nx/vms/api/data/media_server_data.h>

namespace nx {

QnResourcePtr TestResourceFactory::createResource(
    const QnUuid& resourceTypeId,
    const QnResourceParams&)
{
    if (resourceTypeId == nx::vms::api::StorageData::kResourceTypeId)
        return QnResourcePtr(new StorageResourceStub());

    return QnResourcePtr(new nx::CameraResourceStub());
}

} // namespace ec2
