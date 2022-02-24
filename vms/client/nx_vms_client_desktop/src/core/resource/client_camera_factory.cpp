// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "client_camera_factory.h"

#include <core/resource/client_storage_resource.h>
#include <core/resource/client_camera.h>

#include <nx/vms/api/data/analytics_data.h>
#include <nx/vms/api/data/media_server_data.h>
#include <nx/vms/common/resource/analytics_plugin_resource.h>
#include <nx/vms/common/resource/analytics_engine_resource.h>

template<> QnClientResourceFactory* Singleton<QnClientResourceFactory>::s_instance = nullptr;

QnResourcePtr QnClientResourceFactory::createResource(const QnUuid &resourceTypeId,
    const QnResourceParams &)
{
    using namespace nx::vms::common;
    using namespace nx::vms::api;

    if (resourceTypeId == StorageData::kResourceTypeId)
    {
        QnClientStorageResourcePtr result(new QnClientStorageResource());
        result->setActive(true);
        return result;
    }

    if (resourceTypeId == AnalyticsPluginData::kResourceTypeId)
        return AnalyticsPluginResourcePtr(new AnalyticsPluginResource());

    if (resourceTypeId == AnalyticsEngineData::kResourceTypeId)
        return AnalyticsEngineResourcePtr(new AnalyticsEngineResource());

    return QnResourcePtr(new QnClientCameraResource(resourceTypeId));
}

