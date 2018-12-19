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

    void updateFromPluginManifest(
        const nx::vms::api::analytics::PluginManifest& manifest);

    void updateFromEngineManifest(
        const QString& pluginId,
        const QnUuid& engineId,
        const QString& engineName,
        const nx::vms::api::analytics::EngineManifest& manifest);

    void updateFromDeviceAgentManifest(
        const QnUuid& deviceId,
        const QnUuid& engineId,
        const nx::vms::api::analytics::DeviceAgentManifest& manifest);

    std::optional<nx::vms::api::analytics::PluginDescriptor> pluginDescriptor(
        const QString& id) const;
    PluginDescriptorMap pluginDescriptors(const std::set<QString>& pluginIds = {}) const;

    std::optional<nx::vms::api::analytics::EngineDescriptor> engineDescriptor(
        const QnUuid& id) const;
    EngineDescriptorMap engineDescriptors(const std::set<QnUuid>& engineIds = {}) const;

    std::optional <nx::vms::api::analytics::GroupDescriptor> groupDescriptor(
        const QString& id) const;
    GroupDescriptorMap groupDescriptors(const std::set<QString>& groupIds = {}) const;

    std::optional<nx::vms::api::analytics::EventTypeDescriptor> eventTypeDescriptor(
        const QString& id) const;
    EventTypeDescriptorMap eventTypeDescriptors(const std::set<QString>& eventTypeIds = {}) const;
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
        const QString& id) const;
    ObjectTypeDescriptorMap objectTypeDescriptors(
        const std::set<QString>& objectTypeIds = {}) const;
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
        const QString& objectTypeId,
        const QnVirtualCameraResourcePtr& device);

private:
    template<typename DescriptorMap>
    EngineDescriptorMap parentEngineDescriptors(const DescriptorMap& descriptorMap) const;

    template<typename DescriptorMap>
    GroupDescriptorMap parentGroupDescriptors(const DescriptorMap& descriptorMap) const;

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
