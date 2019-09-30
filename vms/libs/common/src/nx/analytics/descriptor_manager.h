#pragma once

#include <set>

#include <common/common_module_aware.h>

#include <nx/analytics/types.h>

// TODO: Move these includes to the actual managers instantiation / usage places.
#include <nx/analytics/plugin_descriptor_manager.h>
#include <nx/analytics/engine_descriptor_manager.h>
#include <nx/analytics/group_descriptor_manager.h>
#include <nx/analytics/event_type_descriptor_manager.h>
#include <nx/analytics/object_type_descriptor_manager.h>
#include <nx/analytics/action_type_descriptor_manager.h>

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
    void commit();
};

} // namespace nx::analytics
