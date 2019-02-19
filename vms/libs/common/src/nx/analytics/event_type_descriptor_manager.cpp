#include "event_type_descriptor_manager.h"
#include <nx/analytics/utils.h>
#include <nx/analytics/properties.h>

#include <common/common_module.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/camera_resource.h>

#include <nx/vms/common/resource/analytics_engine_resource.h>

namespace nx::analytics {

using namespace nx::vms::api::analytics;
using namespace nx::utils::data_structures;

namespace {

static const QString kEventTypeDescriptorTypeName = "EventType";

std::optional<std::set<EventTypeId>> mergeEventTypeIds(
    const std::set<EventTypeId>* first,
    const std::set<EventTypeId>* second)
{
    if (!first && !second)
        return std::nullopt;

    if (!first)
        return *second;

    if (!second)
        return *first;

    std::set<EventTypeId> result = *first;
    result.insert(second->begin(), second->end());
    return result;
}

std::optional<std::set<EventTypeId>> intersectEventTypeIds(
    const std::set<EventTypeId>* first, const std::set<EventTypeId>* second)
{
    if (!first && !second)
        return std::nullopt;

    if (!first)
        return *second;

    if (!second)
        return *first;

    std::set<EventTypeId> result;
    std::set_intersection(
        first->begin(), first->end(),
        second->begin(), second->end(),
        std::inserter(result, result.end()));

    return result;
}

} // namespace

EventTypeDescriptorManager::EventTypeDescriptorManager(QnCommonModule* commonModule):
    base_type(commonModule),
    m_eventTypeDescriptorContainer(
        makeContainer<EventTypeDescriptorContainer>(commonModule, kEventTypeDescriptorsProperty)),
    m_engineDescriptorContainer(
        makeContainer<EngineDescriptorContainer>(commonModule, kEngineDescriptorsProperty)),
    m_groupDescriptorContainer(
        makeContainer<GroupDescriptorContainer>(commonModule, kGroupDescriptorsProperty))
{
}

std::optional<EventTypeDescriptor> EventTypeDescriptorManager::descriptor(
    const EventTypeId& id) const
{
    return fetchDescriptor(m_eventTypeDescriptorContainer, id);
}

EventTypeDescriptorMap EventTypeDescriptorManager::descriptors(
    const std::set<EventTypeId>& eventTypeIds) const
{
    return fetchDescriptors(
        m_eventTypeDescriptorContainer,
        eventTypeIds,
        kEventTypeDescriptorTypeName);
}

ScopedEventTypeIds EventTypeDescriptorManager::supportedEventTypeIds(
    const QnVirtualCameraResourcePtr& device) const
{
    ScopedEventTypeIds result;

    const auto& eventTypeIdsByEngine = device->supportedEventTypes();
    for (const auto& [engineId, eventTypeIds]: eventTypeIdsByEngine)
    {
        for (const auto& eventTypeId: eventTypeIds)
        {
            const auto groupId = eventTypeGroupForEngine(engineId, eventTypeId);
            result[engineId][groupId].insert(eventTypeId);
        }
    }

    return result;
}

ScopedEventTypeIds EventTypeDescriptorManager::supportedEventTypeIdsUnion(
    const QnVirtualCameraResourceList& devices) const
{
    ScopedEventTypeIds result;
    for (const auto& device: devices)
    {
        auto deviceEventTypeIds = supportedEventTypeIds(device);
        MapHelper::merge(&result, deviceEventTypeIds, mergeEventTypeIds);
    }

    return result;
}

ScopedEventTypeIds EventTypeDescriptorManager::supportedEventTypeIdsIntersection(
    const QnVirtualCameraResourceList& devices) const
{
    if (devices.isEmpty())
        return {};

    ScopedEventTypeIds result = supportedEventTypeIds(devices[0]);
    for (auto i = 0; i < devices.size(); ++i)
    {
        const auto deviceEventTypeIds = supportedEventTypeIds(devices[i]);
        MapHelper::intersected(&result, deviceEventTypeIds, intersectEventTypeIds);
    }

    return result;
}

EventTypeDescriptorMap EventTypeDescriptorManager::supportedEventTypeDescriptors(
    const QnVirtualCameraResourcePtr& device) const
{
    std::set<EventTypeId> descriptorIds;

    const auto& eventTypeIdsByEngine = device->supportedEventTypes();
    for (const auto& [engineId, eventTypeIds]: eventTypeIdsByEngine)
        descriptorIds.insert(eventTypeIds.cbegin(), eventTypeIds.cend());

    return descriptors(descriptorIds);
}

ScopedEventTypeIds EventTypeDescriptorManager::compatibleEventTypeIds(
    const QnVirtualCameraResourcePtr& device) const
{
    const auto& compatibleEngines = device->compatibleAnalyticsEngineResources();
    if (compatibleEngines.empty())
        return {};

    ScopedEventTypeIds result;
    for (const auto& engine: compatibleEngines)
    {
        const auto eventTypeDescriptors = engine->analyticsEventTypeDescriptors();
        for (const auto& [eventTypeId, eventTypeDescriptor]: eventTypeDescriptors)
        {
            for (const auto& scope: eventTypeDescriptor.scopes)
                result[scope.engineId][scope.groupId].insert(eventTypeId);
        }
    }

    auto supported = supportedEventTypeIds(device);
    MapHelper::merge(&result, supported, mergeEventTypeIds);

    return result;
}

ScopedEventTypeIds EventTypeDescriptorManager::compatibleEventTypeIdsUnion(
    const QnVirtualCameraResourceList& devices) const
{
    ScopedEventTypeIds result;
    for (const auto& device: devices)
    {
        auto deviceEventTypeIds = compatibleEventTypeIds(device);
        MapHelper::merge(&result, deviceEventTypeIds, mergeEventTypeIds);
    }

    return result;
}

ScopedEventTypeIds EventTypeDescriptorManager::compatibleEventTypeIdsIntersection(
    const QnVirtualCameraResourceList& devices) const
{
    if (devices.isEmpty())
        return {};

    ScopedEventTypeIds result = compatibleEventTypeIds(devices[0]);
    for (auto i = 0; i < devices.size(); ++i)
    {
        const auto deviceEventTypeIds = compatibleEventTypeIds(devices[i]);
        MapHelper::intersected(&result, deviceEventTypeIds, intersectEventTypeIds);
    }

    return result;
}

void EventTypeDescriptorManager::updateFromEngineManifest(
    const PluginId& pluginId,
    const EngineId& engineId,
    const QString& engineName,
    const EngineManifest& manifest)
{
    m_eventTypeDescriptorContainer.mergeWithDescriptors(
        fromManifestItemListToDescriptorMap<EventTypeDescriptor>(engineId, manifest.eventTypes));
}

void EventTypeDescriptorManager::updateFromDeviceAgentManifest(
    const DeviceId& deviceId,
    const EngineId& engineId,
    const DeviceAgentManifest& manifest)
{
    m_eventTypeDescriptorContainer.mergeWithDescriptors(
        fromManifestItemListToDescriptorMap<EventTypeDescriptor>(engineId, manifest.eventTypes));
}

GroupId EventTypeDescriptorManager::eventTypeGroupForEngine(
    const EngineId& engineId,
    const EventTypeId& eventTypeId) const
{
    const auto descriptor = m_eventTypeDescriptorContainer.mergedDescriptors(eventTypeId);
    if (!descriptor)
        return GroupId();

    for (const auto& scope: descriptor->scopes)
    {
        if (scope.engineId == engineId)
            return scope.groupId;
    }

    return GroupId();
}

} // namespace nx::analytics
