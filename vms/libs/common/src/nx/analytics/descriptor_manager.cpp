#include "descriptor_manager.h"

#include <algorithm>

#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>

#include <common/common_module.h>
#include <core/resource_management/resource_pool.h>

namespace nx::analytics {

using namespace nx::vms::api::analytics;

const QString DescriptorManager::kPluginDescriptorsProperty("pluginDescriptors");
const QString DescriptorManager::kEngineDescriptorsProperty("engineDescriptors");
const QString DescriptorManager::kGroupDescriptorsProperty("groupDescriptors");
const QString DescriptorManager::kEventTypeDescriptorsProperty("eventTypeDescriptors");
const QString DescriptorManager::kObjectTypeDescriptorsProperty("objectTypeDescriptors");
const QString DescriptorManager::kActionTypeDescriptorsProperty("actionTypeDescriptors");
const QString DescriptorManager::kDeviceDescriptorsProperty("deviceDescriptors");

namespace {

template <typename Container>
std::unique_ptr<Container> makeContainer(QnCommonModule* commonModule, QString propertyName)
{
    const auto resourcePool = commonModule->resourcePool();
    auto servers = resourcePool->getAllServers(Qn::AnyStatus);

    const auto moduleGuid = commonModule->moduleGUID();
    auto ownResource = resourcePool->getResourceById<QnMediaServerResource>(moduleGuid);

    auto factory = typename Container::StorageFactory(std::move(propertyName));

    return std::make_unique<Container>(std::move(factory), servers, ownResource);
}

template<typename Descriptor, typename Item>
std::map<QString, Descriptor> toMap(const EngineId& engineId, const QList<Item>& items)
{
    std::map<QString, Descriptor> result;
    for (const auto& item: items)
    {
        auto descriptor = Descriptor(engineId, item);
        result.emplace(descriptor.getId(), std::move(descriptor));
    }

    return result;
}

std::set<EventTypeId> eventTypeIdsSupportedByDevice(const QnVirtualCameraResourcePtr& device)
{
    std::set<EventTypeId> supportedEventTypeIds;
    auto supportedEventTypesByEngine = device->supportedAnalyticsEventTypeIds();
    for (const auto& eventTypeIdSet: supportedEventTypesByEngine)
        supportedEventTypeIds.insert(eventTypeIdSet.cbegin(), eventTypeIdSet.cend());

    return supportedEventTypeIds;
}

std::set<ObjectTypeId> objectTypeIdsSupportedByDevice(const QnVirtualCameraResourcePtr& device)
{
    std::set<ObjectTypeId> supportedObjectTypeIds;
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

DescriptorManager::DescriptorManager(QnCommonModule* commonModule):
    base_type(commonModule),
    m_pluginDescriptorContainer(
        makeContainer<detail::Container<PluginDescriptor, PluginId>>(
            commonModule, kPluginDescriptorsProperty)),
    m_engineDescriptorContainer(
        makeContainer<detail::Container<EngineDescriptor, EngineId>>(
            commonModule, kEngineDescriptorsProperty)),
    m_groupDescriptorContainer(
        makeContainer<detail::ScopedContainer<GroupDescriptor, GroupId>>(
            commonModule, kGroupDescriptorsProperty)),
    m_eventTypeDescriptorContainer(
        makeContainer<detail::ScopedContainer<EventTypeDescriptor, EventTypeId>>(
            commonModule, kEventTypeDescriptorsProperty)),
    m_objectTypeDescriptorContainer(
        makeContainer<detail::ScopedContainer<ObjectTypeDescriptor, ObjectTypeId>>(
            commonModule, kObjectTypeDescriptorsProperty)),
    m_actionTypeDescriptorContainer(
        makeContainer<detail::Container<ActionTypeDescriptor, EngineId, ActionTypeId>>(
            commonModule, kActionTypeDescriptorsProperty)),
    m_deviceDescriptorContainer(
        makeContainer<detail::DeviceDescriptorContainer>(
            commonModule, kDeviceDescriptorsProperty))
{
}

void DescriptorManager::clearRuntimeInfo()
{
    m_actionTypeDescriptorContainer->removeDescriptors();
    m_deviceDescriptorContainer->removeDescriptors();
}

void DescriptorManager::updateFromPluginManifest(
    const nx::vms::api::analytics::PluginManifest& manifest)
{
    PluginDescriptor descriptor{manifest.id, manifest.name};
    m_pluginDescriptorContainer->mergeWithDescriptors(std::move(descriptor), manifest.id);
}

void DescriptorManager::updateFromEngineManifest(
    const PluginId& pluginId,
    const EngineId& engineId,
    const QString& engineName,
    const nx::vms::api::analytics::EngineManifest& manifest)
{
    m_engineDescriptorContainer->mergeWithDescriptors(
        EngineDescriptor(engineId, engineName, pluginId), engineId);

    m_actionTypeDescriptorContainer->mergeWithDescriptors(
        toMap<ActionTypeDescriptor>(engineId, manifest.objectActions),
        engineId);

    m_groupDescriptorContainer->mergeWithDescriptors(toMap<GroupDescriptor>(engineId, manifest.groups));
    m_objectTypeDescriptorContainer->mergeWithDescriptors(
        toMap<ObjectTypeDescriptor>(engineId, manifest.objectTypes));
    m_eventTypeDescriptorContainer->mergeWithDescriptors(
        toMap<EventTypeDescriptor>(engineId, manifest.eventTypes));
}

void DescriptorManager::updateFromDeviceAgentManifest(
    const DeviceId& /*deviceId*/,
    const EngineId& engineId,
    const nx::vms::api::analytics::DeviceAgentManifest& manifest)
{
    m_groupDescriptorContainer->mergeWithDescriptors(
        toMap<GroupDescriptor>(engineId, manifest.groups));
    m_objectTypeDescriptorContainer->mergeWithDescriptors(
        toMap<ObjectTypeDescriptor>(engineId, manifest.objectTypes));
    m_eventTypeDescriptorContainer->mergeWithDescriptors(
        toMap<EventTypeDescriptor>(engineId, manifest.eventTypes));
}

void DescriptorManager::addCompatibleAnalyticsEngines(
    const DeviceId& deviceId,
    std::set<EngineId> compatibleEngineIds)
{
    m_deviceDescriptorContainer->mergeWithDescriptors(
        DeviceDescriptor{deviceId, std::move(compatibleEngineIds)}, deviceId);
}

void DescriptorManager::removeCompatibleAnalyticsEngines(
    const DeviceId& deviceId,
    std::set<EngineId> engineIdsToRemove)
{
    if (engineIdsToRemove.empty())
        return;

    auto descriptor = m_deviceDescriptorContainer->mergedDescriptors(deviceId);
    if (!descriptor)
        return;

    for (const auto& engineId: engineIdsToRemove)
        descriptor->compatibleEngines.erase(engineId);

    m_deviceDescriptorContainer->setDescriptors(*descriptor, deviceId);
}

void DescriptorManager::setCompatibleAnalyticsEngines(
    const DeviceId& deviceId,
    std::set<EngineId> engineIds)
{
    m_deviceDescriptorContainer->setDescriptors(
        DeviceDescriptor{deviceId, std::move(engineIds)}, deviceId);
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
        NX_WARNING(
            typeid(DescriptorManager),
            "Unable to fetch %1 descriptor map",
            descriptorTypeName);

        return ReturnType();
    }

    if (ids.empty())
        return *typeDescriptors;

    auto& descriptorMap = *typeDescriptors;
    filterByIds(&descriptorMap, ids);
    return descriptorMap;
}

std::optional<nx::vms::api::analytics::PluginDescriptor> DescriptorManager::pluginDescriptor(
    const PluginId& id) const
{
    return descriptor(m_pluginDescriptorContainer, id);
}

PluginDescriptorMap DescriptorManager::pluginDescriptors(const std::set<PluginId>& pluginIds) const
{
    return descriptors(m_pluginDescriptorContainer, pluginIds, "Plugin");
}

std::optional<nx::vms::api::analytics::EngineDescriptor> DescriptorManager::engineDescriptor(
    const EngineId& id) const
{
    return descriptor(m_engineDescriptorContainer, id);
}

EngineDescriptorMap DescriptorManager::engineDescriptors(const std::set<EngineId>& engineIds) const
{
    return descriptors(m_engineDescriptorContainer, engineIds, "Plugin");
}

std::optional<nx::vms::api::analytics::GroupDescriptor> DescriptorManager::groupDescriptor(
    const GroupId& id) const
{
    return descriptor(m_groupDescriptorContainer, id);
}

GroupDescriptorMap DescriptorManager::groupDescriptors(const std::set<GroupId>& groupIds) const
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
EngineDescriptorMap DescriptorManager::parentEngineDescriptors(
    const DescriptorMap& descriptorMap) const
{
    std::set<QnUuid> engineIds;
    for (const auto& [typeDescriptorId, typeDescriptor]: descriptorMap)
    {
        for (const auto scope : typeDescriptor.scopes)
            engineIds.insert(scope.engineId);
    }

    return engineDescriptors(engineIds);
}

template<typename DescriptorMap>
GroupDescriptorMap DescriptorManager::parentGroupDescriptors(const DescriptorMap& descriptorMap) const
{
    std::set<QString> groupIds;
    for (const auto& [typeDescriptorId, typeDescriptor] : descriptorMap)
    {
        for (const auto scope : typeDescriptor.scopes)
            groupIds.insert(scope.groupId);
    }

    return groupDescriptors(groupIds);
}

std::optional<EventTypeDescriptor> DescriptorManager::eventTypeDescriptor(
    const EventTypeId& id) const
{
    return descriptor(m_eventTypeDescriptorContainer, id);
}

EventTypeDescriptorMap DescriptorManager::eventTypeDescriptors(
    const std::set<EventTypeId>& eventTypeIds) const
{
    return descriptors(m_eventTypeDescriptorContainer, eventTypeIds, "EventType");
}

EventTypeDescriptorMap DescriptorManager::supportedEventTypeDescriptors(
    const QnVirtualCameraResourcePtr& device) const
{
    return supportedByDeviceDescriptors(
        m_eventTypeDescriptorContainer,
        device,
        [](const auto& device) { return eventTypeIdsSupportedByDevice(device); },
        "EventType");
}

EventTypeDescriptorMap DescriptorManager::supportedEventTypeDescriptorsUnion(
    const QnVirtualCameraResourceList& deviceList) const
{
    return supportedTypesUnion(
        m_eventTypeDescriptorContainer,
        deviceList,
        [](const auto& device) { return eventTypeIdsSupportedByDevice(device); },
        "EventType");
}

EventTypeDescriptorMap DescriptorManager::supportedEventTypeDescriptorsIntersection(
    const QnVirtualCameraResourceList& deviceList) const
{
    return supportedTypesIntersection(
        m_eventTypeDescriptorContainer,
        deviceList,
        [](const auto& device) { return eventTypeIdsSupportedByDevice(device); },
        "EventType");
}

EngineDescriptorMap DescriptorManager::eventTypesParentEngineDescriptors(
    const EventTypeDescriptorMap& eventTypeDescriptors) const
{
    return parentEngineDescriptors(eventTypeDescriptors);
}

GroupDescriptorMap DescriptorManager::eventTypesParentGroupDescriptors(
    const EventTypeDescriptorMap& eventTypeDescriptors) const
{
    return parentGroupDescriptors(eventTypeDescriptors);
}

std::optional<nx::vms::api::analytics::ObjectTypeDescriptor>
    DescriptorManager::objectTypeDescriptor(const ObjectTypeId& id) const
{
    return descriptor(m_objectTypeDescriptorContainer, id);
}

ObjectTypeDescriptorMap DescriptorManager::objectTypeDescriptors(
    const std::set<ObjectTypeId>& objectTypeIds) const
{
    return descriptors(m_objectTypeDescriptorContainer, objectTypeIds, "EventType");
}

ObjectTypeDescriptorMap DescriptorManager::supportedObjectTypeDescriptors(
    const QnVirtualCameraResourcePtr& device) const
{
    return supportedByDeviceDescriptors(
        m_objectTypeDescriptorContainer,
        device,
        [](const auto& device) { return objectTypeIdsSupportedByDevice(device); },
        "ObjectType");
}

ObjectTypeDescriptorMap DescriptorManager::supportedObjectTypeDescriptorsUnion(
    const QnVirtualCameraResourceList& deviceList) const
{
    return supportedTypesUnion(
        m_objectTypeDescriptorContainer,
        deviceList,
        [](const auto& device) { return objectTypeIdsSupportedByDevice(device); },
        "ObjectType");
}

ObjectTypeDescriptorMap DescriptorManager::supportedObjectTypeDescriptorsIntersection(
    const QnVirtualCameraResourceList& deviceList) const
{
    return supportedTypesIntersection(
        m_objectTypeDescriptorContainer,
        deviceList,
        [](const auto& device) { return objectTypeIdsSupportedByDevice(device); },
        "ObjectType");
}

EngineDescriptorMap DescriptorManager::objectTypeParentEngineDescriptors(
    const ObjectTypeDescriptorMap& objectTypeDescriptors) const
{
    return parentEngineDescriptors(objectTypeDescriptors);
}

GroupDescriptorMap DescriptorManager::objectTypeParentGroupDescriptors(
    const ObjectTypeDescriptorMap& objectTypeDescriptors) const
{
    return parentGroupDescriptors(objectTypeDescriptors);
}

ActionTypeDescriptorMap DescriptorManager::objectActionTypeDescriptors() const
{
    auto descriptors = m_actionTypeDescriptorContainer->mergedDescriptors();
    return descriptors ? *descriptors : ActionTypeDescriptorMap();
}

ActionTypeDescriptorMap DescriptorManager::availableObjectActionTypeDescriptors(
    const ObjectTypeId& objectTypeId,
    const QnVirtualCameraResourcePtr& device) const
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

std::optional<nx::vms::api::analytics::DeviceDescriptor> DescriptorManager::deviceDescriptor(
    const DeviceId& deviceId) const
{
    return descriptor(m_deviceDescriptorContainer, deviceId);
}

DeviceDescriptorMap DescriptorManager::deviceDescriptors(const std::set<DeviceId>& deviceIds) const
{
    return descriptors(m_deviceDescriptorContainer, deviceIds, "Device");
}

} // namespace nx::analytics
