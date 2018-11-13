#include "test_resource_factory.h"

#include <test_support/resource/storage_resource_stub.h>
#include <test_support/resource/camera_resource_stub.h>

#include <nx/vms/api/data/analytics_data.h>

#include <nx/vms/common/resource/analytics_plugin_resource.h>
#include <nx/vms/common/resource/analytics_engine_resource.h>

#include <nx/vms/api/data/media_server_data.h>

namespace nx {

using namespace nx::vms::api;
using namespace nx::vms::common;

QnResourcePtr TestResourceFactory::createResource(
    const QnUuid& resourceTypeId,
    const QnResourceParams&)
{
    if (resourceTypeId == StorageData::kResourceTypeId)
        return QnResourcePtr(new StorageResourceStub());
    else if (resourceTypeId == AnalyticsPluginData::kResourceTypeId)
        return QnResourcePtr(new AnalyticsPluginResource());
    else if (resourceTypeId == AnalyticsEngineData::kResourceTypeId)
        return QnResourcePtr(new AnalyticsEngineResource());

    return QnResourcePtr(new CameraResourceStub());
}

} // namespace ec2
