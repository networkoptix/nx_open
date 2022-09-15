// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "utils.h"

#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>

namespace nx::analytics {

using namespace nx::vms::api::analytics;

std::set<QString> supportedEventTypeIdsFromManifest(const DeviceAgentManifest& manifest)
{
    std::set<QString> result(
        manifest.supportedEventTypeIds.begin(),
        manifest.supportedEventTypeIds.end());

    for (const auto& eventType: manifest.eventTypes)
        result.insert(eventType.id);

    return result;
}

std::set<QString> supportedObjectTypeIdsFromManifest(const DeviceAgentManifest& manifest)
{
    std::set<QString> result(
        manifest.supportedObjectTypeIds.begin(),
        manifest.supportedObjectTypeIds.end());

    for (const auto& objectType: manifest.objectTypes)
        result.insert(objectType.id);

    return result;
}

bool serverHasActiveObjectEngines(const QnMediaServerResourcePtr& server)
{
    if (!NX_ASSERT(server))
        return false;

    auto cameras = server->resourcePool()->getAllCameras(server);
    return std::any_of(cameras.cbegin(), cameras.cend(),
        [](const auto& camera) { return !camera->supportedObjectTypes().empty(); });
}

} // namespace nx::analytics
