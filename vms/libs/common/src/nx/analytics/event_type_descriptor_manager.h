#pragma once

#include <QtCore/QObject>

#include <nx/analytics/types.h>

#include <nx/vms/api/analytics/descriptors.h>
#include <nx/vms/api/analytics/engine_manifest.h>
#include <nx/vms/api/analytics/device_agent_manifest.h>

#include <core/resource/resource_fwd.h>
#include <common/common_module_aware.h>

namespace nx::analytics {

class EventTypeDescriptorManager: public QObject, public /*mixin*/ QnCommonModuleAware
{
    using base_type = QObject;
    Q_OBJECT

public:
    explicit EventTypeDescriptorManager(QObject* parent = nullptr);

    std::optional<nx::vms::api::analytics::EventTypeDescriptor> descriptor(
        const EventTypeId& id) const;

    EventTypeDescriptorMap descriptors(
        const std::set<EventTypeId>& eventTypeIds = {}) const;

    /**
     * Tree of the Event type ids. Root nodes are Engines, then Groups and Event types as leaves.
     * Includes only those Event types, which are actually available, so only compatible, enabled
     * and running Engines are used.
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
     * Tree of the Event type ids. Root nodes are Engines, then Groups and Event types as leaves.
     * Includes all Event types, which can theoretically be available on this Device, so all
     * compatible Engines are used.
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
    std::unique_ptr<EventTypeDescriptorContainer> m_eventTypeDescriptorContainer;
    std::unique_ptr<EngineDescriptorContainer> m_engineDescriptorContainer;
    std::unique_ptr<GroupDescriptorContainer> m_groupDescriptorContainer;
};

} // namespace nx::analytics
