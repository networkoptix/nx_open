#include "event_type_descriptor_manager.h"
#include <nx/analytics/utils.h>
#include <nx/analytics/properties.h>

#include <common/common_module.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/camera_resource.h>

#include <nx/vms/common/resource/analytics_engine_resource.h>
#include <nx/analytics/helpers.h>

namespace nx::analytics {

using namespace nx::vms::api::analytics;
using namespace nx::utils::data_structures;

static const QString kEventTypeDescriptorTypeName = "EventType";

EventTypeDescriptorManager::EventTypeDescriptorManager(QObject* parent):
    base_type(parent),
    QnCommonModuleAware(parent),
    m_eventTypeDescriptorContainer(
        makeContainer<EventTypeDescriptorContainer>(commonModule(), kEventTypeDescriptorsProperty)),
    m_engineDescriptorContainer(
        makeContainer<EngineDescriptorContainer>(commonModule(), kEngineDescriptorsProperty)),
    m_groupDescriptorContainer(
        makeContainer<GroupDescriptorContainer>(commonModule(), kGroupDescriptorsProperty))
{
}

std::optional<EventTypeDescriptor> EventTypeDescriptorManager::descriptor(
    const EventTypeId& id) const
{
    return fetchDescriptor(m_eventTypeDescriptorContainer.get(), id);
}

EventTypeDescriptorMap EventTypeDescriptorManager::descriptors(
    const std::set<EventTypeId>& eventTypeIds) const
{
    return fetchDescriptors(
        m_eventTypeDescriptorContainer.get(),
        eventTypeIds,
        kEventTypeDescriptorTypeName);
}

ScopedEventTypeIds EventTypeDescriptorManager::supportedEventTypeIds(
    const QnVirtualCameraResourcePtr& device) const
{
    ScopedEventTypeIds result;

    const auto& eventTypeIdsByEngine = device->supportedEventTypes();

    std::set<EngineId> allEngineIds;
    std::set<EventTypeId> allEventTypeIds;
    for (const auto& [engineId, eventTypeIds]: eventTypeIdsByEngine)
    {
        allEngineIds.insert(engineId);
        allEventTypeIds.insert(eventTypeIds.begin(), eventTypeIds.end());
    }

    return eventTypeGroupsByEngines(allEngineIds, allEventTypeIds);
}

ScopedEventTypeIds EventTypeDescriptorManager::supportedEventTypeIdsUnion(
    const QnVirtualCameraResourceList& devices) const
{
    ScopedEventTypeIds result;
    for (const auto& device: devices)
    {
        auto deviceEventTypeIds = supportedEventTypeIds(device);
        MapHelper::merge(&result, deviceEventTypeIds, mergeEntityIds);
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
        MapHelper::intersected(&result, deviceEventTypeIds, intersectEntityIds);
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
        const bool engineIsEnabled =
            device->enabledAnalyticsEngines().contains(engine->getId());

        if (engineIsEnabled || engine->isDeviceDependent())
        {
            auto supported = supportedEventTypeIds(device);
            MapHelper::merge(&result, supported, mergeEntityIds);
        }
        else
        {
            const auto eventTypeDescriptors = engine->analyticsEventTypeDescriptors();
            for (const auto& [eventTypeId, eventTypeDescriptor]: eventTypeDescriptors)
            {
                for (const auto& scope: eventTypeDescriptor.scopes)
                    result[scope.engineId][scope.groupId].insert(eventTypeId);
            }
        }
    }

    return result;
}

ScopedEventTypeIds EventTypeDescriptorManager::compatibleEventTypeIdsUnion(
    const QnVirtualCameraResourceList& devices) const
{
    ScopedEventTypeIds result;
    for (const auto& device: devices)
    {
        auto deviceEventTypeIds = compatibleEventTypeIds(device);
        MapHelper::merge(&result, deviceEventTypeIds, mergeEntityIds);
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
        MapHelper::intersected(&result, deviceEventTypeIds, intersectEntityIds);
    }

    return result;
}

void EventTypeDescriptorManager::updateFromEngineManifest(
    const PluginId& pluginId,
    const EngineId& engineId,
    const QString& engineName,
    const EngineManifest& manifest)
{
    m_eventTypeDescriptorContainer->mergeWithDescriptors(
        fromManifestItemListToDescriptorMap<EventTypeDescriptor>(engineId, manifest.eventTypes));
}

void EventTypeDescriptorManager::updateFromDeviceAgentManifest(
    const DeviceId& deviceId,
    const EngineId& engineId,
    const DeviceAgentManifest& manifest)
{
    m_eventTypeDescriptorContainer->mergeWithDescriptors(
        fromManifestItemListToDescriptorMap<EventTypeDescriptor>(engineId, manifest.eventTypes));
}

ScopedEventTypeIds EventTypeDescriptorManager::eventTypeGroupsByEngines(
    const std::set<EngineId>& engineIds,
    const std::set<EventTypeId>& eventTypeIds) const
{
    ScopedEventTypeIds result;
    const auto descriptors = this->descriptors(eventTypeIds);
    for (const auto& [eventTypeId, descriptor]: descriptors)
    {
        for (const auto& scope: descriptor.scopes)
        {
            if (engineIds.find(scope.engineId) != engineIds.cend())
                result[scope.engineId][scope.groupId].insert(eventTypeId);
        }
    }

    return result;
}

} // namespace nx::analytics
