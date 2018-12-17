#include "helper.h"

#include <algorithm>

#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>

#include <common/common_module.h>
#include <core/resource_management/resource_pool.h>

namespace nx::analytics {

using namespace nx::vms::api::analytics;

const QString Helper::kPluginDescriptorsProperty("pluginDescriptors");
const QString Helper::kEngineDescriptorsProperty("engineDescriptors");
const QString Helper::kGroupDescriptorsProperty("groupDescriptors");
const QString Helper::kEventTypeDescriptorsProperty("eventTypeDescriptors");
const QString Helper::kObjectTypeDescriptorsProperty("objectTypeDescriptors");
const QString Helper::kActionTypeDescriptorsProperty("actionTypeDescriptors");

namespace {

template<template<typename...> typename Container, typename Descriptor, typename... Scopes>
std::unique_ptr<Container<Descriptor, Scopes...>> makeContainer(
    QnCommonModule* commonModule,
    QString propertyName)
{
    const auto resourcePool = commonModule->resourcePool();
    auto servers = resourcePool->getAllServers(Qn::AnyStatus);

    const auto moduleGuid = commonModule->moduleGUID();
    auto ownResource = resourcePool->getResourceById<QnMediaServerResource>(moduleGuid);

    auto factory = DefaultDescriptorStorageFactory<Descriptor, Scopes...>(std::move(propertyName));
    return std::make_unique<Container<Descriptor, Scopes...>>(
        std::move(factory),
        servers,
        ownResource);
}

template<typename Descriptor, typename Item>
std::map<QString, Descriptor> toMap(const QnUuid& engineId, const QList<Item>& items)
{
    std::map<QString, Descriptor> result;
    for (const auto& item: items)
    {
        auto descriptor = Descriptor(engineId, item);
        result.emplace(descriptor.getId(), std::move(descriptor));
    }

    return result;
}

std::set<QString> eventTypeIdsSupportedByDevice(const QnVirtualCameraResourcePtr& device)
{
    std::set<QString> supportedEventTypeIds;
    auto supportedEventTypesByEngine = device->supportedAnalyticsEventTypeIds();
    for (const auto& eventTypeIdSet: supportedEventTypesByEngine)
        supportedEventTypeIds.insert(eventTypeIdSet.cbegin(), eventTypeIdSet.cend());

    return supportedEventTypeIds;
}

std::set<QString> objectTypeIdsSupportedByDevice(const QnVirtualCameraResourcePtr& device)
{
    std::set<QString> supportedObjectTypeIds;
    auto supportedObjectTypesByEngine = device->supportedAnalyticsObjectTypeIds();
    for (const auto& eventTypeIdSet: supportedObjectTypesByEngine)
        supportedObjectTypeIds.insert(eventTypeIdSet.cbegin(), eventTypeIdSet.cend());

    return supportedObjectTypeIds;
}

template<typename Key, typename Value>
void filterByIds(std::map<Key, Value>* inOutMapToFilter, const std::set<Key>& keys)
{
    auto it = inOutMapToFilter->begin();
    while (it != inOutMapToFilter->end())
    {
        if (keys.find(it->first) == keys.end())
            it = inOutMapToFilter->erase(it);
        else
            ++it;
    }
}

} // namespace

using namespace nx::vms::api::analytics;

Helper::Helper(QnCommonModule* commonModule):
    base_type(commonModule),
    m_pluginDescriptorContainer(
        makeContainer<
            detail::Container,
            PluginDescriptor,
            QString>(commonModule, kPluginDescriptorsProperty)),
    m_engineDescriptorContainer(
        makeContainer<
            detail::Container,
            EngineDescriptor,
            QnUuid>(commonModule, kEngineDescriptorsProperty)),
    m_groupDescriptorContainer(
        makeContainer<
            detail::ScopedContainer,
            GroupDescriptor,
            QString>(commonModule, kGroupDescriptorsProperty)),
    m_eventTypeDescriptorContainer(
        makeContainer<
            detail::ScopedContainer,
            EventTypeDescriptor,
            QString>(commonModule, kEventTypeDescriptorsProperty)),
    m_objectTypeDescriptorContainer(
        makeContainer<
            detail::ScopedContainer,
            ObjectTypeDescriptor,
            QString>(commonModule, kObjectTypeDescriptorsProperty)),
    m_actionTypeDescriptorContainer(
        makeContainer<
            detail::Container,
            ActionTypeDescriptor,
            QnUuid, QString>(commonModule, kActionTypeDescriptorsProperty))
{
}

void Helper::clearRuntimeInfo()
{
    m_actionTypeDescriptorContainer->removeDescriptors();
}

void Helper::updateFromManifest(const nx::vms::api::analytics::PluginManifest& manifest)
{
    PluginDescriptor descriptor{manifest.id, manifest.name};
    m_pluginDescriptorContainer->addDescriptors(std::move(descriptor), manifest.id);
}

void Helper::updateFromManifest(
    const QString& pluginId,
    const QnUuid& engineId,
    const QString& engineName,
    const nx::vms::api::analytics::EngineManifest& manifest)
{
    m_engineDescriptorContainer->addDescriptors(
        EngineDescriptor(engineId, engineName, pluginId), engineId);

    m_actionTypeDescriptorContainer->addDescriptors(
        toMap<ActionTypeDescriptor>(engineId, manifest.objectActions),
        engineId);

    m_groupDescriptorContainer->addDescriptors(toMap<GroupDescriptor>(engineId, manifest.groups));
    m_objectTypeDescriptorContainer->addDescriptors(
        toMap<ObjectTypeDescriptor>(engineId, manifest.objectTypes));
    m_eventTypeDescriptorContainer->addDescriptors(
        toMap<EventTypeDescriptor>(engineId, manifest.eventTypes));
}

void Helper::updateFromManifest(
    const QnUuid& /*deviceId*/,
    const QnUuid& engineId,
    const nx::vms::api::analytics::DeviceAgentManifest& manifest)
{
    m_groupDescriptorContainer->addDescriptors(
        toMap<GroupDescriptor>(engineId, manifest.groups));
    m_objectTypeDescriptorContainer->addDescriptors(
        toMap<ObjectTypeDescriptor>(engineId, manifest.objectTypes));
    m_eventTypeDescriptorContainer->addDescriptors(
        toMap<EventTypeDescriptor>(engineId, manifest.eventTypes));
}

template<typename Container, typename Key>
auto descriptor(const Container& container, const Key& id)
{
    return container->mergedDescriptors(id);
}

template<typename Container, typename KeyType>
auto descriptors(
    const Container& container,
    const std::set<KeyType>& ids,
    const QString& descriptorTypeName)
{
    using ReturnType = typename decltype(container->mergedDescriptors())::value_type;
    auto typeDescriptors = container->mergedDescriptors();
    if (!typeDescriptors)
    {
        NX_WARNING(typeid(Helper), "Unable to fetch %1 descriptor map", descriptorTypeName);
        return ReturnType();
    }

    if (ids.empty())
        return *typeDescriptors;

    auto& descriptorMap = *typeDescriptors;
    filterByIds(&descriptorMap, ids);
    return descriptorMap;
}

std::optional<nx::vms::api::analytics::PluginDescriptor> Helper::plugin(const QString& id) const
{
    return descriptor(m_pluginDescriptorContainer, id);
}

PluginDescriptorMap Helper::plugins(const std::set<QString>& pluginIds) const
{
    return descriptors(m_pluginDescriptorContainer, pluginIds, "Plugin");
}

std::optional<nx::vms::api::analytics::EngineDescriptor> Helper::engine(const QnUuid& id) const
{
    return descriptor(m_engineDescriptorContainer, id);
}

EngineDescriptorMap Helper::engines(const std::set<QnUuid>& engineIds) const
{
    return descriptors(m_engineDescriptorContainer, engineIds, "Plugin");
}

std::optional <nx::vms::api::analytics::GroupDescriptor> Helper::group(const QString& id) const
{
    return descriptor(m_groupDescriptorContainer, id);
}

GroupDescriptorMap Helper::groups(const std::set<QString>& groupIds) const
{
    return descriptors(m_groupDescriptorContainer, groupIds, "Group");
}

template<typename Container>
auto supportedByDeviceDescriptors(
    const Container& container,
    const QnVirtualCameraResourcePtr& device,
    std::function<std::set<QString>(const QnVirtualCameraResourcePtr&)> supportedTypeIdsFetcher,
    const QString& descriptorTypeName)
{
    std::set<QString> supportedTypeIds = supportedTypeIdsFetcher(device);

    auto descriptorMap = descriptors(container, supportedTypeIds, descriptorTypeName);
    filterByIds(&descriptorMap, supportedTypeIds);

    return descriptorMap;
}

template<typename Container>
auto supportedTypesUnion(
    const Container& container,
    const QnVirtualCameraResourceList& deviceList,
    std::function<std::set<QString>(const QnVirtualCameraResourcePtr&)> supportedTypeIdsFetcher,
    const QString& descriptorTypeName)
{
    std::set<QString> unitedTypeIds;
    for (const auto& device: deviceList)
    {
        auto supportedTypeIds = supportedTypeIdsFetcher(device);
        unitedTypeIds.insert(supportedTypeIds.cbegin(), supportedTypeIds.cend());
    }

    auto descriptorMap = descriptors(container, std::set<QString>{}, descriptorTypeName);
    filterByIds(&descriptorMap, unitedTypeIds);
    return descriptorMap;
}

template<typename Container>
auto supportedTypesIntersection(
    const Container& container,
    const QnVirtualCameraResourceList& deviceList,
    std::function<std::set<QString>(const QnVirtualCameraResourcePtr&)> supportedTypeIdsFetcher,
    const QString& descriptorTypeName)
{
    using ResultType = typename decltype(container->mergedDescriptors())::value_type;
    if (deviceList.isEmpty())
        return ResultType();

    std::set<QString> typeIdIntersection = supportedTypeIdsFetcher(deviceList[0]);
    for (auto i = 1; i < deviceList.size(); ++i)
    {
        auto supportedTypeIds = supportedTypeIdsFetcher(deviceList[i]);
        for (auto it = typeIdIntersection.begin(); it != typeIdIntersection.end();)
        {
            if (supportedTypeIds.find(*it) == supportedTypeIds.cend())
                it = typeIdIntersection.erase(it);
            else
                ++it;
        }
    }

    if (typeIdIntersection.empty())
        return ResultType();

    auto descriptorMap = descriptors(container, std::set<QString>{}, descriptorTypeName);
    filterByIds(&descriptorMap, typeIdIntersection);
    return descriptorMap;
}

template<typename DescriptorMap>
EngineDescriptorMap Helper::parentEngines(const DescriptorMap& descriptorMap) const
{
    std::set<QnUuid> engineIds;
    for (const auto&[typeDescriptorId, typeDescriptor] : descriptorMap)
    {
        for (const auto scope : typeDescriptor.scopes)
            engineIds.insert(scope.engineId);
    }

    return engines(engineIds);
}

template<typename DescriptorMap>
GroupDescriptorMap Helper::parentGroups(const DescriptorMap& descriptorMap) const
{
    std::set<QString> groupIds;
    for (const auto&[typeDescriptorId, typeDescriptor] : descriptorMap)
    {
        for (const auto scope : typeDescriptor.scopes)
            groupIds.insert(scope.groupId);
    }

    return groups(groupIds);
}

std::optional<EventTypeDescriptor> Helper::eventType(const QString& id) const
{
    return descriptor(m_eventTypeDescriptorContainer, id);
}

EventTypeDescriptorMap Helper::eventTypes(const std::set<QString>& eventTypeIds) const
{
    return descriptors(m_eventTypeDescriptorContainer, eventTypeIds, "EventType");
}

EventTypeDescriptorMap Helper::supportedEventTypes(
    const QnVirtualCameraResourcePtr& device) const
{
    return supportedByDeviceDescriptors(
        m_eventTypeDescriptorContainer,
        device,
        [](const auto& device) { return eventTypeIdsSupportedByDevice(device); },
        "EventType");
}

EventTypeDescriptorMap Helper::supportedEventTypesUnion(
    const QnVirtualCameraResourceList& deviceList) const
{
    return supportedTypesUnion(
        m_eventTypeDescriptorContainer,
        deviceList,
        [](const auto& device) { return eventTypeIdsSupportedByDevice(device); },
        "EventType");
}

EventTypeDescriptorMap Helper::supportedEventTypesIntersection(
    const QnVirtualCameraResourceList& deviceList) const
{
    return supportedTypesIntersection(
        m_eventTypeDescriptorContainer,
        deviceList,
        [](const auto& device) { return eventTypeIdsSupportedByDevice(device); },
        "EventType");
}

EngineDescriptorMap Helper::parentEventTypeEngines(
    const EventTypeDescriptorMap& eventTypeDescriptors) const
{
    return parentEngines(eventTypeDescriptors);
}

GroupDescriptorMap Helper::parentEventTypeGroups(
    const EventTypeDescriptorMap& eventTypeDescriptors) const
{
    return parentGroups(eventTypeDescriptors);
}

std::optional<nx::vms::api::analytics::ObjectTypeDescriptor> Helper::objectType(
    const QString& id) const
{
    return descriptor(m_objectTypeDescriptorContainer, id);
}

ObjectTypeDescriptorMap Helper::objectTypes(const std::set<QString>& objectTypeIds) const
{
    return descriptors(m_objectTypeDescriptorContainer, objectTypeIds, "EventType");
}

ObjectTypeDescriptorMap Helper::supportedObjectTypes(
    const QnVirtualCameraResourcePtr& device) const
{
    return supportedByDeviceDescriptors(
        m_objectTypeDescriptorContainer,
        device,
        [](const auto& device) { return objectTypeIdsSupportedByDevice(device); },
        "ObjectType");
}

ObjectTypeDescriptorMap Helper::supportedObjectTypesUnion(
    const QnVirtualCameraResourceList& deviceList) const
{
    return supportedTypesUnion(
        m_objectTypeDescriptorContainer,
        deviceList,
        [](const auto& device) { return objectTypeIdsSupportedByDevice(device); },
        "ObjectType");
}

ObjectTypeDescriptorMap Helper::supportedObjectTypesIntersection(
    const QnVirtualCameraResourceList& deviceList) const
{
    return supportedTypesIntersection(
        m_objectTypeDescriptorContainer,
        deviceList,
        [](const auto& device) { return objectTypeIdsSupportedByDevice(device); },
        "ObjectType");
}

EngineDescriptorMap Helper::parentObjectTypeEngines(
    const ObjectTypeDescriptorMap& objectTypeDescriptors) const
{
    return parentEngines(objectTypeDescriptors);
}

GroupDescriptorMap Helper::parentObjectTypeGroups(
    const ObjectTypeDescriptorMap& objectTypeDescriptors) const
{
    return parentGroups(objectTypeDescriptors);
}

ActionTypeDescriptorMap Helper::actionTypes() const
{
    auto descriptors = m_actionTypeDescriptorContainer->mergedDescriptors();
    return descriptors ? *descriptors : ActionTypeDescriptorMap();
}

ActionTypeDescriptorMap Helper::availableObjectActions(
    const QString& objectTypeId,
    const QnVirtualCameraResourcePtr& device)
{
    const auto resourcePool = commonModule()->resourcePool();
    const auto deviceParentServer = resourcePool->getResourceById<QnMediaServerResource>(
        device->getParentId());

    if (!deviceParentServer)
    {
        NX_WARNING(
            this,
            "Device %1(%2) parent server doesn't exist",
            device->getUserDefinedName(), device->getId());
        return {};
    }

    const auto serverStatus = deviceParentServer->getStatus();
    if (serverStatus == Qn::Offline
        || serverStatus == Qn::Incompatible
        || serverStatus == Qn::Unauthorized)
    {
        NX_WARNING(
            this,
            "Device %1(%2), Device server status is %2, analytics actions are not available",
            device->getUserDefinedName(), device->getId());

        return {};
    }

    auto descriptors = m_actionTypeDescriptorContainer->descriptors(deviceParentServer->getId());
    if (!descriptors)
    {
        NX_DEBUG(this, "Device %1(%2), Object type id %3, no available actions found",
            device->getUserDefinedName(), device->getId(), objectTypeId);
        return {};
    }

    return MapHelper::filtered(
        *descriptors,
        [&objectTypeId](const auto& keys, const auto& descriptor)
        {
            return descriptor.supportedObjectTypeIds.contains(objectTypeId);
        });

}

} // namespace nx::analytics
