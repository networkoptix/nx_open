#pragma once

#include <set>

#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <common/common_module.h>
#include <common/common_module_aware.h>

#include <analytics/detected_objects_storage/analytics_events_storage_types.h>

#include <nx/analytics/multiresource_descriptor_container.h>
#include <nx/analytics/replacement_merge_executor.h>
#include <nx/analytics/property_descriptor_storage_factory.h>
#include <nx/analytics/multiresource_descriptor_container.h>
#include <nx/analytics/scoped_merge_executor.h>
#include <nx/analytics/device_descriptor_merge_executor.h>
#include <nx/analytics/types.h>

#include <nx/vms/api/analytics/descriptors.h>
#include <nx/vms/api/analytics/plugin_manifest.h>
#include <nx/vms/api/analytics/engine_manifest.h>
#include <nx/vms/api/analytics/device_agent_manifest.h>

namespace nx::analytics {

namespace detail {

template<typename Descriptor, typename... Scopes>
using Container = MultiresourceDescriptorContainer<
    PropertyDescriptorStorageFactory<Descriptor, Scopes...>,
    ReplacementMergeExecutor<Descriptor>>;

template<typename Descriptor, typename... Scopes>
using ScopedContainer = MultiresourceDescriptorContainer<
    PropertyDescriptorStorageFactory<Descriptor, Scopes...>,
    ScopedMergeExecutor<Descriptor>>;

using DeviceDescriptorContainer = MultiresourceDescriptorContainer<
    PropertyDescriptorStorageFactory<nx::vms::api::analytics::DeviceDescriptor, DeviceId>,
    DeviceDescriptorMergeExecutor>;

template<typename Descriptor, typename... Scopes>
using ContainerPtr = std::unique_ptr<Container<Descriptor, Scopes...>>;

template<typename Descriptor, typename... Scopes>
using ScopedContainerPtr = std::unique_ptr<ScopedContainer<Descriptor, Scopes...>>;

using DeviceDescriptorContainerPtr = std::unique_ptr<DeviceDescriptorContainer>;

} // namespace detail

class DescriptorManager: public QnCommonModuleAware
{
    using base_type = QnCommonModuleAware;

public:
    DescriptorManager(QnCommonModule* commonModule);

    static const QString kPluginDescriptorsProperty;
    static const QString kEngineDescriptorsProperty;
    static const QString kGroupDescriptorsProperty;
    static const QString kEventTypeDescriptorsProperty;
    static const QString kObjectTypeDescriptorsProperty;
    static const QString kActionTypeDescriptorsProperty;
    static const QString kDeviceDescriptorsProperty;

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

    void addCompatibleAnalyticsEngines(
        const DeviceId& deviceId,
        std::set<EngineId> compatibleEngineIds);

    void removeCompatibleAnalyticsEngines(
        const DeviceId& deviceId,
        std::set<EngineId> engineIdsToRemove);

    void setCompatibleAnalyticsEngines(
        const DeviceId& deviceId,
        std::set<EngineId> engineIdsToRemove);

    std::optional<nx::vms::api::analytics::PluginDescriptor> pluginDescriptor(
        const PluginId& id) const;
    PluginDescriptorMap pluginDescriptors(const std::set<PluginId>& pluginIds = {}) const;

    std::optional<nx::vms::api::analytics::EngineDescriptor> engineDescriptor(
        const EngineId& id) const;
    EngineDescriptorMap engineDescriptors(const std::set<EngineId>& engineIds = {}) const;

    std::optional <nx::vms::api::analytics::GroupDescriptor> groupDescriptor(
        const GroupId& id) const;
    GroupDescriptorMap groupDescriptors(const std::set<GroupId>& groupIds = {}) const;

    std::optional<nx::vms::api::analytics::EventTypeDescriptor> eventTypeDescriptor(
        const EventTypeId& id) const;
    EventTypeDescriptorMap eventTypeDescriptors(
        const std::set<EventTypeId>& eventTypeIds = {}) const;
    EventTypeDescriptorMap supportedEventTypeDescriptors(
        const QnVirtualCameraResourcePtr& device) const;
    EventTypeDescriptorMap supportedEventTypeDescriptorsUnion(
        const QnVirtualCameraResourceList& deviceList) const;
    EventTypeDescriptorMap supportedEventTypeDescriptorsIntersection(
        const QnVirtualCameraResourceList& deviceList) const;

    EngineDescriptorMap eventTypesParentEngineDescriptors(
        const EventTypeDescriptorMap& eventTypeDescriptors) const;

    GroupDescriptorMap eventTypesParentGroupDescriptors(
        const EventTypeDescriptorMap& eventTypeDescriptors) const;

    std::optional<nx::vms::api::analytics::ObjectTypeDescriptor> objectTypeDescriptor(
        const ObjectTypeId& id) const;
    ObjectTypeDescriptorMap objectTypeDescriptors(
        const std::set<ObjectTypeId>& objectTypeIds = {}) const;
    ObjectTypeDescriptorMap supportedObjectTypeDescriptors(
        const QnVirtualCameraResourcePtr& device) const;
    ObjectTypeDescriptorMap supportedObjectTypeDescriptorsUnion(
        const QnVirtualCameraResourceList& deviceList) const;
    ObjectTypeDescriptorMap supportedObjectTypeDescriptorsIntersection(
        const QnVirtualCameraResourceList& deviceList) const;

    EngineDescriptorMap objectTypeParentEngineDescriptors(
        const ObjectTypeDescriptorMap& objectTypeDescriptors) const;

    GroupDescriptorMap objectTypeParentGroupDescriptors(
        const ObjectTypeDescriptorMap& objectTypeDescriptors) const;

    ActionTypeDescriptorMap objectActionTypeDescriptors() const;

    ActionTypeDescriptorMap availableObjectActionTypeDescriptors(
        const ObjectTypeId& objectTypeId,
        const QnVirtualCameraResourcePtr& device) const;

    std::optional<nx::vms::api::analytics::DeviceDescriptor> deviceDescriptor(
        const DeviceId& deviceId) const;

    DeviceDescriptorMap deviceDescriptors(const std::set<DeviceId>& deviceIds) const;

private:
    template<typename DescriptorMap>
    EngineDescriptorMap parentEngineDescriptors(const DescriptorMap& descriptorMap) const;

    template<typename DescriptorMap>
    GroupDescriptorMap parentGroupDescriptors(const DescriptorMap& descriptorMap) const;

private:
    detail::ContainerPtr<
        nx::vms::api::analytics::PluginDescriptor,
        PluginId> m_pluginDescriptorContainer;

    detail::ContainerPtr<
        nx::vms::api::analytics::EngineDescriptor,
        EngineId> m_engineDescriptorContainer;

    detail::ScopedContainerPtr<
        nx::vms::api::analytics::GroupDescriptor,
        GroupId> m_groupDescriptorContainer;

    detail::ScopedContainerPtr<
        nx::vms::api::analytics::EventTypeDescriptor,
        EventTypeId> m_eventTypeDescriptorContainer;

    detail::ScopedContainerPtr<
        nx::vms::api::analytics::ObjectTypeDescriptor,
        ObjectTypeId> m_objectTypeDescriptorContainer;

    detail::ContainerPtr<
        nx::vms::api::analytics::ActionTypeDescriptor,
        EngineId, ActionTypeId> m_actionTypeDescriptorContainer;

    detail::DeviceDescriptorContainerPtr m_deviceDescriptorContainer;
};

} // namespace nx::analytics
