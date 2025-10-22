// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "utils.h"

#include <ranges>

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
    const QString& integrationActionId, const QnResourcePool* resourcePool)
{
    const auto engines = resourcePool->getResources<nx::vms::common::AnalyticsEngineResource>();
    auto it = std::ranges::find_if(engines,
        [&integrationActionId](const auto& e)
        {
            auto manifest = e->manifest();
            return std::ranges::any_of(
                manifest.integrationActions,
                [&integrationActionId](const auto& ia) { return ia.id == integrationActionId; });
        });

    return it != engines.end()
        ? *it
        : QnSharedResourcePointer<nx::vms::common::AnalyticsEngineResource>();
}

std::optional<IntegrationAction> findIntegrationActionDescriptorById(
    const QString& integrationActionId,
    const nx::vms::common::AnalyticsEngineResourceList& engines)
{
    for (const auto& engine : engines)
    {
        const auto manifest = engine->manifest();
        for (const auto& integrationAction: manifest.integrationActions)
        {
            if (integrationAction.id == integrationActionId)
                return integrationAction;
        }
    }
    return std::nullopt;
}

} // namespace nx::analytics
