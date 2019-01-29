#include "action_type_descriptor_manager.h"

#include <nx/analytics/properties.h>
#include <nx/analytics/utils.h>

#include <common/common_module.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/camera_resource.h>

#include <nx/utils/log/log.h>

namespace nx::analytics {

using namespace nx::vms::api::analytics;

ActionTypeDescriptorManager::ActionTypeDescriptorManager(QnCommonModule* commonModule):
    base_type(commonModule),
    m_actionTypeDescriptorContainer(
        makeContainer<ActionTypeDescriptorContainer>(commonModule, kActionTypeDescriptorsProperty))
{
}

std::optional<ActionTypeDescriptor> ActionTypeDescriptorManager::descriptor(
    const EngineId& engineId,
    const ActionTypeId& actionTypeId) const
{
    return m_actionTypeDescriptorContainer.mergedDescriptors(engineId, actionTypeId);
}

void ActionTypeDescriptorManager::updateFromEngineManifest(
    const PluginId& pluginId,
    const EngineId& engineId,
    const QString& engineName,
    const EngineManifest& manifest)
{
    m_actionTypeDescriptorContainer.mergeWithDescriptors(
        fromManifestItemListToDescriptorMap<ActionTypeDescriptor>(
            engineId, manifest.objectActions),
        engineId);
}

void ActionTypeDescriptorManager::clearRuntimeInfo()
{
    m_actionTypeDescriptorContainer.removeDescriptors();
}

ActionTypeDescriptorMap ActionTypeDescriptorManager::availableObjectActionTypeDescriptors(
    const ObjectTypeId& objectTypeId,
    const QnVirtualCameraResourcePtr& device) const
{
    const auto resourcePool = commonModule()->resourcePool();
    const auto deviceParentServer =
        resourcePool->getResourceById<QnMediaServerResource>(device->getParentId());

    if (!deviceParentServer)
    {
        NX_WARNING(this, "Device %1(%2) parent server doesn't exist", device->getUserDefinedName(),
            device->getId());
        return {};
    }

    const auto serverStatus = deviceParentServer->getStatus();
    if (serverStatus == Qn::Offline || serverStatus == Qn::Incompatible ||
        serverStatus == Qn::Unauthorized)
    {
        NX_WARNING(this,
            "Device %1(%2), Device server status is %2, analytics actions are not available",
            device->getUserDefinedName(), device->getId());

        return {};
    }

    auto descriptors = m_actionTypeDescriptorContainer.descriptors(deviceParentServer->getId());
    if (!descriptors)
    {
        NX_DEBUG(this, "Device %1(%2), Object type id %3, no available actions found",
            device->getUserDefinedName(), device->getId(), objectTypeId);
        return {};
    }

    return MapHelper::filtered(
        *descriptors, [&objectTypeId](const auto& keys, const auto& descriptor) {
            return descriptor.supportedObjectTypeIds.contains(objectTypeId);
        });
}

} // namespace nx::analytics
