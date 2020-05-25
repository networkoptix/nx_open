#include "utils.h"
#include <core/resource/camera_resource.h>

namespace nx::analytics {

using namespace nx::vms::api::analytics;

namespace {

bool cameraHasActiveObjectEngines(const QnVirtualCameraResourcePtr& camera)
{
    if (!camera)
        return false;

    QSet<QnUuid> objectEngines;
    for (const auto& objectType : camera->supportedObjectTypes())
    {
        if (!objectType.second.empty())
            objectEngines.insert(objectType.first);
    }
    const auto usedObjectEngines = camera->enabledAnalyticsEngines().intersect(objectEngines);
    return !usedObjectEngines.empty();
}

} // namespace

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

bool serverHasActiveObjectEngines(QnCommonModule* commonModule, const QnUuid& serverId)
{
    auto server = commonModule->resourcePool()->getResourceById<QnMediaServerResource>(serverId);
    if (!server)
        return false;

    auto cameras = commonModule->resourcePool()->getAllCameras(server);
    for (const auto& camera : cameras)
    {
        if (cameraHasActiveObjectEngines(camera))
            return true;
    }
    return false;
}

bool cameraHasActiveObjectEngines(QnCommonModule* commonModule, const QnUuid& cameraId)
{
    return cameraHasActiveObjectEngines(
        commonModule->resourcePool()->getResourceById<QnVirtualCameraResource>(cameraId));
}

} // namespace nx::analytics
