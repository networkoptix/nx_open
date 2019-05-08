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

bool hasActiveObjectEngines(QnCommonModule* commonModule, const QnUuid& serverId)
{
    auto server = commonModule->resourcePool()->getResourceById<QnMediaServerResource>(
        commonModule->moduleGUID());
    if (!server)
        return false;

    bool hasAnalyticsEngine = false;
    auto cameras = commonModule->resourcePool()->getAllCameras(server);
    for (const auto& camera : cameras)
    {
        QSet<QnUuid> objectEngines;
        for (const auto& objectType : camera->supportedObjectTypes())
        {
            if (!objectType.second.empty())
                objectEngines.insert(objectType.first);
        }
        auto usedObjectEngines = camera->enabledAnalyticsEngines().intersect(objectEngines);
        if (!usedObjectEngines.empty())
            return true;
    }
    return false;
}

} // namespace nx::analytics
