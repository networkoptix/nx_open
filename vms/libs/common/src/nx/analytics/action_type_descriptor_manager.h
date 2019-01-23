#pragma once

#include <nx/analytics/types.h>

#include <nx/vms/api/analytics/descriptors.h>
#include <nx/vms/api/analytics/engine_manifest.h>
#include <nx/vms/api/analytics/device_agent_manifest.h>

#include <common/common_module_aware.h>
#include <core/resource/resource_fwd.h>

namespace nx::analytics {

class ActionTypeDescriptorManager : public QnCommonModuleAware
{
    using base_type = QnCommonModuleAware;

public:
    ActionTypeDescriptorManager(QnCommonModule* commonModule);

    std::optional<nx::vms::api::analytics::ActionTypeDescriptor> descriptor(
        const EngineId& engineId,
        const ActionTypeId& actionTypeId) const;

    void updateFromEngineManifest(
        const PluginId& pluginId,
        const EngineId& engineId,
        const QString& engineName,
        const nx::vms::api::analytics::EngineManifest& manifest);

    void clearRuntimeInfo();

    ActionTypeDescriptorMap availableObjectActionTypeDescriptors(
        const ObjectTypeId& objectTypeId,
        const QnVirtualCameraResourcePtr& device) const;

private:
    ActionTypeDescriptorContainer m_actionTypeDescriptorContainer;
};

} // namespace nx::analytics
