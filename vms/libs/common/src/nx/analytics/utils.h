#pragma once

#include <QtCore/QList>

#include <nx/analytics/types.h>
#include <nx/vms/api/analytics/device_agent_manifest.h>

#include <common/common_module.h>
#include <core/resource_management/resource_pool.h>

#include <core/resource/media_server_resource.h>

namespace nx::analytics {

template <typename Descriptor, typename Item>
std::map<QString, Descriptor> fromManifestItemListToDescriptorMap(
    const nx::analytics::EngineId& engineId, const QList<Item>& items)
{
    std::map<QString, Descriptor> result;
    for (const auto& item: items)
    {
        auto descriptor = Descriptor(engineId, item);
        result.emplace(descriptor.getId(), std::move(descriptor));
    }

    return result;
}

template <typename Container, typename Key>
auto fetchDescriptor(const Container& container, const Key& id)
{
    return container.mergedDescriptors(id);
}

template <typename Key, typename Value>
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

template <typename Container, typename KeyType>
auto fetchDescriptors(
    const Container& container,
    const std::set<KeyType>& ids,
    const QString& descriptorTypeName)
{
    using ReturnType = typename decltype(container.mergedDescriptors())::value_type;
    auto typeDescriptors = container.mergedDescriptors();
    if (!typeDescriptors)
        return ReturnType();

    if (ids.empty())
        return *typeDescriptors;

    auto& descriptorMap = *typeDescriptors;
    filterByIds(&descriptorMap, ids);
    return descriptorMap;
}

template <typename Container>
Container makeContainer(QnCommonModule* commonModule, QString propertyName)
{
    const auto resourcePool = commonModule->resourcePool();
    auto servers = resourcePool->getAllServers(Qn::AnyStatus);

    const auto moduleGuid = commonModule->moduleGUID();
    auto ownResource = resourcePool->getResourceById<QnMediaServerResource>(moduleGuid);

    auto factory = typename Container::StorageFactory(std::move(propertyName));
    return Container(std::move(factory), servers, ownResource);
}

std::set<EventTypeId> supportedEventTypeIdsFromManifest(
    const nx::vms::api::analytics::DeviceAgentManifest& manifest);

std::set<EventTypeId> supportedObjectTypeIdsFromManifest(
    const nx::vms::api::analytics::DeviceAgentManifest& manifest);

std::map<EngineId, std::set<EventTypeId>> eventTypeIdsSupportedByDevice(
    const DeviceDescriptorContainer& container, const DeviceId& deviceId);

std::map<EngineId, std::set<ObjectTypeId>> objectTypeIdsSupportedByDevice(
    const DeviceDescriptorContainer& container, const DeviceId& deviceId);

} // namespace nx::analytics
