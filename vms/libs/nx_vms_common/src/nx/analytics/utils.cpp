// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "utils.h"

#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/common/resource/analytics_engine_resource.h>

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

bool serverHasActiveObjectEngines(const QnMediaServerResourcePtr& server)
{
    if (!NX_ASSERT(server))
        return false;

    auto cameras = server->resourcePool()->getAllCameras(server);
    return std::ranges::any_of(
        cameras,
        [](const auto& camera) { return !camera->supportedObjectTypes().empty(); });
}

nx::vms::common::AnalyticsEngineResourcePtr findEngineByIntegrationActionId(
    const QString& integrationAction, const QnResourcePool* resourcePool)
{
    const auto engines = resourcePool->getResources<nx::vms::common::AnalyticsEngineResource>();
    auto it = std::find_if(engines.begin(), engines.end(),
        [&integrationAction](const auto& e)
        {
            auto manifest = e->manifest();
            return std::ranges::any_of(
                manifest.objectActions,
                [&integrationAction](const auto& oa) { return oa.id == integrationAction; });
        });

    return it != engines.end()
        ? *it
        : QnSharedResourcePointer<nx::vms::common::AnalyticsEngineResource>();
}

std::optional<QJsonObject> findSettingsModelByIntegrationActionId(
    const QString& integrationAction,
    const nx::vms::common::AnalyticsEngineResourceList& engines)
{
    for (const auto& engine : engines)
    {
        const auto manifest = engine->manifest();
        for (const auto& objectAction : manifest.objectActions)
        {
            if (objectAction.id == integrationAction)
                return objectAction.parametersModel;
        }
    }
    return std::nullopt;
}

} // namespace nx::analytics
