// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "utils.h"
#include <core/resource/camera_resource.h>

namespace nx::analytics {

using namespace nx::vms::api::analytics;

std::set<QString> supportedEventTypeIdsFromManifest(const DeviceAgentManifest& manifest)
{
    std::set<QString> result(
        manifest.supportedEventTypeIds.begin(),
        manifest.supportedEventTypeIds.end());

    for (const auto& eventType: manifest.eventTypes)
        result.insert(eventType.id);

    for (const auto& entityType: manifest.supportedTypes)
    {
        if (!entityType.eventTypeId.isEmpty())
            result.insert(entityType.eventTypeId);
    }

    return result;
}

std::set<QString> supportedObjectTypeIdsFromManifest(const DeviceAgentManifest& manifest)
{
    std::set<QString> result(
        manifest.supportedObjectTypeIds.begin(),
        manifest.supportedObjectTypeIds.end());

    for (const auto& objectType: manifest.objectTypes)
        result.insert(objectType.id);

    for (const auto& entityType: manifest.supportedTypes)
    {
        if (!entityType.objectTypeId.isEmpty())
            result.insert(entityType.objectTypeId);
    }

    return result;
}

bool serverHasActiveObjectEngines(QnCommonModule* commonModule, const QnUuid& serverId)
{
    auto server = commonModule->resourcePool()->getResourceById<QnMediaServerResource>(serverId);
    if (!server)
        return false;

    auto cameras = commonModule->resourcePool()->getAllCameras(server);
    return std::any_of(cameras.cbegin(), cameras.cend(),
        [](const auto& camera) { return !camera->supportedObjectTypes().empty(); });
}

} // namespace nx::analytics
