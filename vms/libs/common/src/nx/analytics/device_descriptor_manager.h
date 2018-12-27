#pragma once

#include <nx/analytics/types.h>

#include <nx/vms/api/analytics/descriptors.h>
#include <nx/vms/api/analytics/device_agent_manifest.h>

#include <common/common_module_aware.h>
#include <core/resource/resource_fwd.h>

namespace nx::analytics {

class DeviceDescriptorManager: public QnCommonModuleAware
{
    using base_type = QnCommonModuleAware;

public:
    DeviceDescriptorManager(QnCommonModule* commonModule);

    std::optional<nx::vms::api::analytics::DeviceDescriptor> descriptor(const DeviceId& id) const;

    DeviceDescriptorMap descriptors(const std::set<DeviceId>& deviceIds = {}) const;

    void updateFromDeviceAgentManifest(
        const DeviceId& deviceId,
        const EngineId& engineId,
        const nx::vms::api::analytics::DeviceAgentManifest& manifest);

    void clearRuntimeInfo();

    void removeDeviceDescriptors(const std::set<DeviceId>& deviceIds);

    void addCompatibleAnalyticsEngines(
        const DeviceId& deviceId,
        std::set<EngineId> compatibleEngineIds);

    void removeCompatibleAnalyticsEngines(
        const DeviceId& deviceId,
        std::set<EngineId> engineIdsToRemove);

    void removeCompatibleAnalyticsEngines(const std::set<EngineId>& engineIds);

    void setCompatibleAnalyticsEngines(
        const DeviceId& deviceId,
        std::set<EngineId> engineIds);

private:
    nx::vms::api::analytics::DeviceDescriptor deviceDescriptor(const DeviceId& deviceId) const;

    void updateSupportedEventTypes(
        const DeviceId& deviceId,
        const EngineId& engineId,
        const nx::vms::api::analytics::DeviceAgentManifest& manifest);

    void updateSupportedObjectTypes(
        const DeviceId& deviceId,
        const EngineId& engineId,
        const nx::vms::api::analytics::DeviceAgentManifest& manifest);

private:
    DeviceDescriptorContainer m_deviceDescriptorContainer;
};

} // namespace nx::analytics
