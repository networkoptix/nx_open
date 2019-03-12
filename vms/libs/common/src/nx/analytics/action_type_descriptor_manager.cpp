#include "action_type_descriptor_manager.h"

#include <nx/analytics/properties.h>
#include <nx/analytics/utils.h>
#include <nx/vms/common/resource/analytics_engine_resource.h>

#include <common/common_module.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/camera_resource.h>

#include <api/runtime_info_manager.h>

#include <nx/utils/log/log.h>

namespace nx::analytics {

using namespace nx::vms::api::analytics;

namespace {

std::map<ActionTypeId, ActionTypeDescriptor> retrieveActionTypeDescriptors(
    const EngineId& engineId,
    const nx::vms::api::analytics::EngineManifest& engineManifest,
    const ObjectTypeId& objectTypeId)
{
    std::map<ActionTypeId, ActionTypeDescriptor> result;
    for (const auto& actionType: engineManifest.objectActions)
    {
        if (!actionType.supportedObjectTypeIds.contains(objectTypeId))
            continue;

        ActionTypeDescriptor descriptor(engineId, actionType);
        result[descriptor.getId()] = descriptor;
    }

    return result;
}

} // namespace

ActionTypeDescriptorManager::ActionTypeDescriptorManager(QnCommonModule* commonModule):
    base_type(commonModule)
{
}

std::optional<ActionTypeDescriptor> ActionTypeDescriptorManager::descriptor(
    const ActionTypeId& actionTypeId) const
{
    const auto resourcePool = commonModule()->resourcePool();
    const auto engines = resourcePool->getResources<nx::vms::common::AnalyticsEngineResource>();

    for (const auto& engine: engines)
    {
        const auto manifest = engine->manifest();
        for (const auto& actionType: manifest.objectActions)
        {
            if (actionType.id == actionTypeId)
                return ActionTypeDescriptor(engine->getId(), actionType);
        }
    }

    return std::nullopt;
}

ActionTypeDescriptorMap ActionTypeDescriptorManager::availableObjectActionTypeDescriptors(
    const ObjectTypeId& objectTypeId,
    const QnVirtualCameraResourcePtr& device) const
{
    const auto deviceParentServer = device->getParentServer();
    if (!NX_ASSERT(deviceParentServer))
        return {};

    const auto runtimeInfoManager = commonModule()->runtimeInfoManager();
    const auto runtimeInfo = runtimeInfoManager->item(deviceParentServer->getId());

    const auto resourcePool = commonModule()->resourcePool();
    const auto engines = resourcePool->getResourcesByIds<nx::vms::common::AnalyticsEngineResource>(
        runtimeInfo.data.activeAnalyticsEngines);

    ActionTypeDescriptorMap result;
    for (const auto& engine: engines)
    {
        const auto manifest = engine->manifest();
        auto actionTypeDescriptors = retrieveActionTypeDescriptors(
            engine->getId(),
            manifest,
            objectTypeId);

        if (!actionTypeDescriptors.empty())
            result.emplace(engine->getId(), std::move(actionTypeDescriptors));
    }

    return result;
}

} // namespace nx::analytics
