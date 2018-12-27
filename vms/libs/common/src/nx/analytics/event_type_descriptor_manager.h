#pragma once

#include <nx/analytics/types.h>

#include <nx/vms/api/analytics/descriptors.h>
#include <nx/vms/api/analytics/engine_manifest.h>
#include <nx/vms/api/analytics/device_agent_manifest.h>

#include <core/resource/resource_fwd.h>
#include <common/common_module_aware.h>

namespace nx::analytics {

class EventTypeDescriptorManager: public QnCommonModuleAware
{
    using base_type = QnCommonModuleAware;

public:
    EventTypeDescriptorManager(QnCommonModule* commonModule);

    std::optional<nx::vms::api::analytics::EventTypeDescriptor> descriptor(
        const EventTypeId& id) const;

    EventTypeDescriptorMap descriptors(
        const std::set<EventTypeId>& eventTypeIds = {}) const;

    ScopedEventTypeIds supportedEventTypeIds(
        const QnVirtualCameraResourcePtr& device) const;

    ScopedEventTypeIds supportedEventTypeIdsUnion(
        const QnVirtualCameraResourceList& devices) const;

    ScopedEventTypeIds supportedEventTypeIdsIntersection(
        const QnVirtualCameraResourceList& devices) const;

    EventTypeDescriptorMap supportedEventTypeDescriptors(
        const QnVirtualCameraResourcePtr& device) const;

    ScopedEventTypeIds compatibleEventTypeIds(
        const QnVirtualCameraResourcePtr& device) const;

    ScopedEventTypeIds compatibleEventTypeIdsUnion(
        const QnVirtualCameraResourceList& devices) const;

    ScopedEventTypeIds compatibleEventTypeIdsIntersection(
        const QnVirtualCameraResourceList& devices) const;

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
    GroupId eventTypeGroupForEngine(
        const EngineId& engineId,
        const EventTypeId& eventTypeId) const;

private:
    EventTypeDescriptorContainer m_eventTypeDescriptorContainer;
    EngineDescriptorContainer m_engineDescriptorContainer;
    GroupDescriptorContainer m_groupDescriptorContainer;
    DeviceDescriptorContainer m_deviceDescriptorContainer;
};

} // namespace nx::analytics
