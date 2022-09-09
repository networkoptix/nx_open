// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QList>

#include <core/resource/resource_fwd.h>
#include <nx/analytics/taxonomy/descriptor_container.h>
#include <nx/vms/api/analytics/descriptors.h>
#include <nx/vms/api/analytics/device_agent_manifest.h>

namespace nx::analytics {

template <typename Descriptor, typename Item>
std::map<QString, Descriptor> fromManifestItemListsToDescriptorMap(
    const nx::vms::api::analytics::EngineId& engineId,
    const std::vector<const QList<Item>*>& itemLists)
{
    std::map<QString, Descriptor> result;

    for (const auto itemList: itemLists)
    {
        for (const auto& item: *itemList)
        {
            auto descriptor = Descriptor(engineId, item);
            result.emplace(descriptor.getId(), std::move(descriptor));
        }
    }

    return result;
}

template<typename Item>
std::set<QString> fromManifestItemListToIds(
    const std::vector<const QList<Item>*>& itemLists)
{
    std::set<QString> result;
    for (const QList<Item>* const itemList: itemLists)
    {
        for (const auto& item: *itemList)
            result.insert(item.id);
    }

    return result;
}

template<typename Item>
std::set<QString> calculateSupportedTypeIds(
    const QList<QString>& supportedTypeIds,
    const QList<Item>& freeDeclaredTypes)
{
    std::set<QString> result;

    result.insert(supportedTypeIds.begin(), supportedTypeIds.end());

    for (const Item& item: freeDeclaredTypes)
        result.insert(item.id);

    return result;
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

template<typename DescriptorMap>
DescriptorMap filterDescriptorMap(
    DescriptorMap descriptorMap,
    const std::set<typename DescriptorMap::key_type>& ids)
{
    if (ids.empty())
        return descriptorMap;

    DescriptorMap result;
    for (const auto& id: ids)
    {
        if (auto itr = descriptorMap.find(id); itr != descriptorMap.cend())
            result[id] = std::move(itr->second);
    }

    return result;
}

template <typename Descriptor, typename Key>
auto fetchDescriptors(taxonomy::DescriptorContainer* container, const std::set<Key>& ids)
{
    using namespace nx::vms::api::analytics;

    Descriptors descriptors = container->descriptors();
    if constexpr (std::is_same_v<Descriptor, EventTypeDescriptor>)
        return filterDescriptorMap(std::move(descriptors.eventTypeDescriptors), ids);
    if constexpr (std::is_same_v<Descriptor, ObjectTypeDescriptor>)
        return filterDescriptorMap(std::move(descriptors.objectTypeDescriptors), ids);
    if constexpr (std::is_same_v<Descriptor, ColorTypeDescriptor>)
        return filterDescriptorMap(std::move(descriptors.colorTypeDescriptors), ids);
    if constexpr (std::is_same_v<Descriptor, EnumTypeDescriptor>)
        return filterDescriptorMap(std::move(descriptors.enumTypeDescriptors), ids);
    if constexpr (std::is_same_v<Descriptor, EngineDescriptor>)
        return filterDescriptorMap(std::move(descriptors.engineDescriptors), ids);
    if constexpr (std::is_same_v<Descriptor, PluginDescriptor>)
        return filterDescriptorMap(std::move(descriptors.pluginDescriptors), ids);
    if constexpr (std::is_same_v<Descriptor, GroupDescriptor>)
        return filterDescriptorMap(std::move(descriptors.groupDescriptors), ids);

    NX_ASSERT(false, "Unknown descriptor type: %1", typeid(Descriptor));
    return std::map<Key, Descriptor>();
}

template <typename Descriptor, typename Key>
std::optional<Descriptor> fetchDescriptor(taxonomy::DescriptorContainer* container, const Key& id)
{
    auto descriptorMap = fetchDescriptors<Descriptor>(container, std::set<Key>{id});

    if (auto itr = descriptorMap.find(id); itr != descriptorMap.cend())
        return std::optional<Descriptor>(std::move(itr->second));

    return std::nullopt;
}

/*
 * Return true if at least one analytics engine with objects detection is activated for server.
 */
NX_VMS_COMMON_API bool serverHasActiveObjectEngines(const QnMediaServerResourcePtr& server);

NX_VMS_COMMON_API std::set<QString> supportedObjectTypeIdsFromManifest(
    const nx::vms::api::analytics::DeviceAgentManifest& manifest);

NX_VMS_COMMON_API std::set<QString> supportedEventTypeIdsFromManifest(
    const nx::vms::api::analytics::DeviceAgentManifest& manifest);

} // namespace nx::analytics
