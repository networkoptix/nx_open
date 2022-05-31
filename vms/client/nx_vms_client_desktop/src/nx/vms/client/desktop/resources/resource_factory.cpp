// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "resource_factory.h"

#include <core/resource/client_storage_resource.h>
#include <core/resource/client_camera.h>

#include <nx/vms/api/data/analytics_data.h>
#include <nx/vms/api/data/media_server_data.h>
#include <nx/vms/common/resource/analytics_plugin_resource.h>
#include <nx/vms/common/resource/analytics_engine_resource.h>

namespace nx::vms::client::desktop {

QnResourcePtr ResourceFactory::createResource(
    const QnUuid& resourceTypeId,
    const QnResourceParams& /*params*/)
{
    using namespace nx::vms::common;

    if (resourceTypeId == api::StorageData::kResourceTypeId)
    {
        QnClientStorageResourcePtr result(new QnClientStorageResource());
        result->setActive(true);
        return result;
    }

    if (resourceTypeId == api::AnalyticsPluginData::kResourceTypeId)
        return AnalyticsPluginResourcePtr(new AnalyticsPluginResource());

    if (resourceTypeId == api::AnalyticsEngineData::kResourceTypeId)
        return AnalyticsEngineResourcePtr(new AnalyticsEngineResource());

    return QnResourcePtr(new QnClientCameraResource(resourceTypeId));
}

} // namespace nx::vms::client::desktop
