#pragma once

#include <nx/analytics/types.h>

#include <nx/vms/api/analytics/descriptors.h>
#include <nx/vms/api/analytics/engine_manifest.h>
#include <nx/vms/api/analytics/device_agent_manifest.h>

#include <core/resource/resource_fwd.h>
#include <common/common_module_aware.h>

namespace nx::analytics {

class EventTypeDescriptorManager: public /*mixin*/ QnCommonModuleAware
{
    using base_type = QnCommonModuleAware;

public:
    EventTypeDescriptorManager(QnCommonModule* commonModule);

    std::optional<nx::vms::api::analytics::EventTypeDescriptor> descriptor(
        const EventTypeId& id) const;

    EventTypeDescriptorMap descriptors(
        const std::set<EventTypeId>& eventTypeIds = {}) const;

    /**
     * Tree of the event type ids. Root nodes are engines, then groups and event types as leaves.
     * Includes only those event types, which are actually available, so only compatible, enabled
     * and running engines are used.
     */
    ScopedEventTypeIds supportedEventTypeIds(
        const QnVirtualCameraResourcePtr& device) const;

    ScopedEventTypeIds supportedEventTypeIdsUnion(
        const QnVirtualCameraResourceList& devices) const;

    ScopedEventTypeIds supportedEventTypeIdsIntersection(
        const QnVirtualCameraResourceList& devices) const;

    EventTypeDescriptorMap supportedEventTypeDescriptors(
        const QnVirtualCameraResourcePtr& device) const;

    /**
     * Tree of the event type ids. Root nodes are engines, then groups and event types as leaves.
     * Includes all event types, which can theoretically be available on this device, so all
     * compatible engines are used.
     */
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
    ScopedEventTypeIds eventTypeGroupsByEngines(
        const std::set<EngineId>& engineIds,
        const std::set<EventTypeId>& eventTypeIds) const;

private:
    EventTypeDescriptorContainer m_eventTypeDescriptorContainer;
    EngineDescriptorContainer m_engineDescriptorContainer;
    GroupDescriptorContainer m_groupDescriptorContainer;
};

} // namespace nx::analytics
