#include "mobile_client_camera_factory.h"
#include "mobile_client_camera.h"

#include <client_core/client_core_module.h>
#include <core/resource/resource_type.h>
#include <core/resource/client_storage_resource.h>

#include <nx/vms/api/data/analytics_data.h>
#include <nx/vms/api/data/media_server_data.h>

#include <nx/vms/common/resource/analytics_plugin_resource.h>
#include <nx/vms/common/resource/analytics_engine_resource.h>

QnResourcePtr QnMobileClientCameraFactory::createResource(const QnUuid &resourceTypeId, const QnResourceParams &)
{
    using namespace nx::vms::common;
    using namespace nx::vms::api;

    if (resourceTypeId == StorageData::kResourceTypeId)
        return QnResourcePtr(new QnClientStorageResource(qnClientCoreModule->commonModule()));

    if (resourceTypeId == AnalyticsPluginData::kResourceTypeId)
        return AnalyticsPluginResourcePtr(new AnalyticsPluginResource());

    if (resourceTypeId == AnalyticsEngineData::kResourceTypeId)
        return AnalyticsEngineResourcePtr(new AnalyticsEngineResource());

    /* Currently we support only cameras. */
    QnResourceTypePtr resourceType = qnResTypePool->getResourceType(resourceTypeId);
    if (!resourceType.isNull() && !resourceType->isCamera())
        return QnResourcePtr();

    return QnResourcePtr(new QnMobileClientCamera(resourceTypeId));
}
