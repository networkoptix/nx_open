#pragma once

#include <set>

#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <common/common_module.h>
#include <common/common_module_aware.h>

#include <analytics/detected_objects_storage/analytics_events_storage_types.h>

#include <nx/analytics/multiresource_descriptor_container.h>
#include <nx/analytics/default_descriptor_storage.h>
#include <nx/analytics/default_descriptor_merger.h>
#include <nx/analytics/default_descriptor_storage_factory.h>
#include <nx/analytics/multiresource_descriptor_container.h>
#include <nx/analytics/scoped_descriptor_merger.h>

#include <nx/vms/api/analytics/descriptors.h>
#include <nx/vms/api/analytics/plugin_manifest.h>
#include <nx/vms/api/analytics/engine_manifest.h>
#include <nx/vms/api/analytics/device_agent_manifest.h>

namespace nx::analytics {

namespace detail {

template<typename Descriptor, typename... Scopes>
using Container = MultiresourceDescriptorContainer<
    DefaultDescriptorStorageFactory<Descriptor, Scopes...>,
    DefaultDescriptorMerger<Descriptor>>;

template<typename Descriptor, typename... Scopes>
using ScopedContainer = MultiresourceDescriptorContainer<
    DefaultDescriptorStorageFactory<Descriptor, Scopes...>,
    ScopedDescriptorMerger<Descriptor>>;

template<typename Descriptor, typename... Scopes>
using ContainerPtr = std::unique_ptr<Container<Descriptor, Scopes...>>;

template<typename Descriptor, typename... Scopes>
using ScopedContainerPtr = std::unique_ptr<ScopedContainer<Descriptor, Scopes...>>;

} // namespace detail

using PluginDescriptorMap = std::map<QString, nx::vms::api::analytics::PluginDescriptor>;
using EngineDescriptorMap = std::map<QnUuid, nx::vms::api::analytics::EngineDescriptor>;
using GroupDescriptorMap = std::map<QString, nx::vms::api::analytics::GroupDescriptor>;
using EventTypeDescriptorMap = std::map<QString, nx::vms::api::analytics::EventTypeDescriptor>;
using ObjectTypeDescriptorMap = std::map<QString, nx::vms::api::analytics::ObjectTypeDescriptor>;

using ActionTypeDescriptorMap = MapHelper::NestedMap<
    std::map,
    QnUuid,
    QString,
    nx::vms::api::analytics::ActionTypeDescriptor>;

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

    void clearRuntimeInfo();

    void updateFromManifest(
        const nx::vms::api::analytics::PluginManifest& manifest);

    void updateFromManifest(
        const QString& pluginId,
        const QnUuid& engineId,
        const QString& engineName,
        const nx::vms::api::analytics::EngineManifest& manifest);

    void updateFromManifest(
        const QnUuid& deviceId,
        const QnUuid& engineId,
        const nx::vms::api::analytics::DeviceAgentManifest& manifest);

    std::optional<nx::vms::api::analytics::PluginDescriptor> plugin(const QString& id) const;
    PluginDescriptorMap plugins(const std::set<QString>& pluginIds = {}) const;

    std::optional<nx::vms::api::analytics::EngineDescriptor> engine(const QnUuid& id) const;
    EngineDescriptorMap engines(const std::set<QnUuid>& engineIds = {}) const;

    std::optional <nx::vms::api::analytics::GroupDescriptor> group(const QString& id) const;
    GroupDescriptorMap groups(const std::set<QString>& groupIds = {}) const;

    std::optional<nx::vms::api::analytics::EventTypeDescriptor> eventType(const QString& id) const;
    EventTypeDescriptorMap eventTypes(const std::set<QString>& eventTypeIds = {}) const;
    EventTypeDescriptorMap supportedEventTypes(const QnVirtualCameraResourcePtr& device) const;
    EventTypeDescriptorMap supportedEventTypesUnion(
        const QnVirtualCameraResourceList& deviceList) const;
    EventTypeDescriptorMap supportedEventTypesIntersection(
        const QnVirtualCameraResourceList& deviceList) const;

    EngineDescriptorMap parentEventTypeEngines(
        const EventTypeDescriptorMap& eventTypeDescriptors) const;

    GroupDescriptorMap parentEventTypeGroups(
        const EventTypeDescriptorMap& eventTypeDescriptors) const;

    std::optional<nx::vms::api::analytics::ObjectTypeDescriptor> objectType(
        const QString& id) const;
    ObjectTypeDescriptorMap objectTypes(const std::set<QString>& objectTypeIds = {}) const;
    ObjectTypeDescriptorMap supportedObjectTypes(const QnVirtualCameraResourcePtr& device) const;
    ObjectTypeDescriptorMap supportedObjectTypesUnion(
        const QnVirtualCameraResourceList& deviceList) const;
    ObjectTypeDescriptorMap supportedObjectTypesIntersection(
        const QnVirtualCameraResourceList& deviceList) const;

    EngineDescriptorMap parentObjectTypeEngines(
        const ObjectTypeDescriptorMap& objectTypeDescriptors) const;

    GroupDescriptorMap parentObjectTypeGroups(
        const ObjectTypeDescriptorMap& objectTypeDescriptors) const;

    ActionTypeDescriptorMap actionTypes() const;

    ActionTypeDescriptorMap availableObjectActions(
        const QString& objectTypeId,
        const QnVirtualCameraResourcePtr& device);

private:
    template<typename DescriptorMap>
    EngineDescriptorMap parentEngines(const DescriptorMap& descriptorMap) const;

    template<typename DescriptorMap>
    GroupDescriptorMap parentGroups(const DescriptorMap& descriptorMap) const;

private:
    detail::ContainerPtr<
        nx::vms::api::analytics::PluginDescriptor,
        QString> m_pluginDescriptorContainer;

    detail::ContainerPtr<
        nx::vms::api::analytics::EngineDescriptor,
        QnUuid> m_engineDescriptorContainer;

    detail::ScopedContainerPtr<
        nx::vms::api::analytics::GroupDescriptor,
        QString> m_groupDescriptorContainer;

    detail::ScopedContainerPtr<
        nx::vms::api::analytics::EventTypeDescriptor,
        QString> m_eventTypeDescriptorContainer;

    detail::ScopedContainerPtr<
        nx::vms::api::analytics::ObjectTypeDescriptor,
        QString> m_objectTypeDescriptorContainer;

    detail::ContainerPtr<
        nx::vms::api::analytics::ActionTypeDescriptor,
        QnUuid, QString> m_actionTypeDescriptorContainer;
};

} // namespace nx::analytics
