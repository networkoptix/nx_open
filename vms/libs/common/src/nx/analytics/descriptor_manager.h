#pragma once

#include <set>

#include <common/common_module_aware.h>

#include <nx/analytics/types.h>

#include <nx/analytics/plugin_descriptor_manager.h>
#include <nx/analytics/engine_descriptor_manager.h>
#include <nx/analytics/group_descriptor_manager.h>
#include <nx/analytics/event_type_descriptor_manager.h>
#include <nx/analytics/object_type_descriptor_manager.h>
#include <nx/analytics/action_type_descriptor_manager.h>
#include <nx/analytics/device_descriptor_manager.h>

#include <nx/vms/api/analytics/descriptors.h>
#include <nx/vms/api/analytics/plugin_manifest.h>
#include <nx/vms/api/analytics/engine_manifest.h>
#include <nx/vms/api/analytics/device_agent_manifest.h>

namespace nx::analytics {

class DescriptorManager: public /*mixin*/ QnCommonModuleAware
{
    using base_type = QnCommonModuleAware;

public:
    DescriptorManager(QnCommonModule* commonModule);

    void clearRuntimeInfo();

    void updateFromPluginManifest(
        const nx::vms::api::analytics::PluginManifest& manifest);

    void updateFromEngineManifest(
        const PluginId& pluginId,
        const EngineId& engineId,
        const QString& engineName,
        const nx::vms::api::analytics::EngineManifest& manifest);

    void updateFromDeviceAgentManifest(
        const DeviceId& deviceId,
        const EngineId& engineId,
        const nx::vms::api::analytics::DeviceAgentManifest& manifest);

private:
    PluginDescriptorManager m_pluginDescriptorManager;
    EngineDescriptorManager m_engineDescriptorManager;
    GroupDescriptorManager m_groupDescriptorManager;
    EventTypeDescriptorManager m_eventTypeDescriptorManager;
    ObjectTypeDescriptorManager m_objectTypeDescriptorManager;
    ActionTypeDescriptorManager m_actionTypeDescriptorManager;
    DeviceDescriptorManager m_deviceDescriptorManager;
};

} // namespace nx::analytics
