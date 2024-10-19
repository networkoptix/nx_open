// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "mobile_client_resource_factory.h"

#include <core/resource/resource_type.h>
#include <nx/vms/api/data/analytics_data.h>
#include <nx/vms/api/data/media_server_data.h>
#include <nx/vms/common/resource/analytics_engine_resource.h>
#include <nx/vms/common/resource/analytics_plugin_resource.h>

#include "mobile_client_camera.h"

template<>
QnMobileClientResourceFactory* Singleton<QnMobileClientResourceFactory>::s_instance = nullptr;

QnResourcePtr QnMobileClientResourceFactory::createResource(
    const nx::Uuid &resourceTypeId,
    const QnResourceParams &)
{
    using namespace nx::vms::common;
    using namespace nx::vms::api;

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
