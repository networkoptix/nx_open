// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "test_resource_factory.h"

#include <nx/vms/api/data/analytics_data.h>
#include <nx/vms/api/data/media_server_data.h>
#include <nx/vms/common/resource/analytics_engine_resource.h>
#include <nx/vms/common/resource/analytics_plugin_resource.h>

#include "camera_resource_stub.h"
#include "storage_resource_stub.h"

namespace nx::vms::common::test {

using namespace nx::vms::api;

QnResourcePtr TestResourceFactory::createResource(
    const nx::Uuid& resourceTypeId,
    const QnResourceParams&)
{
    if (resourceTypeId == StorageData::kResourceTypeId)
        return QnResourcePtr(new StorageResourceStub());

    if (resourceTypeId == AnalyticsPluginData::kResourceTypeId)
        return QnResourcePtr(new AnalyticsPluginResource());

    if (resourceTypeId == AnalyticsEngineData::kResourceTypeId)
        return QnResourcePtr(new AnalyticsEngineResource());

    return QnResourcePtr(new CameraResourceStub());
}

} // namespace nx::vms::common::test
