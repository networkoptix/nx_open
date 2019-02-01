#include "object_type_descriptor_manager.h"

#include <nx/analytics/properties.h>
#include <nx/analytics/utils.h>

#include <common/common_module.h>
#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>

#include <nx/vms/common/resource/analytics_engine_resource.h>

namespace nx::analytics {

namespace {

const QString kObjectTypeDescriptorTypeName("ObjectType");

std::optional<std::set<ObjectTypeId>> mergeObjectTypeIds(
    const std::set<ObjectTypeId>* first, const std::set<ObjectTypeId>* second)
{
    if (!first && !second)
        return std::nullopt;

    if (!first)
        return *second;

    if (!second)
        return *first;

    std::set<ObjectTypeId> result = *first;
    result.insert(second->begin(), second->end());
    return result;
}

std::optional<std::set<ObjectTypeId>> intersectObjectTypeIds(
    const std::set<ObjectTypeId>* first, const std::set<ObjectTypeId>* second)
{
    if (!first && !second)
        return std::nullopt;

    if (!first)
        return *second;

    if (!second)
        return *first;

    std::set<ObjectTypeId> result;
    std::set_intersection(first->begin(), first->end(), second->begin(), second->end(),
        std::inserter(result, result.end()));

    return result;
}

} // namespace

using namespace nx::vms::api::analytics;
using namespace nx::utils::data_structures;

ObjectTypeDescriptorManager::ObjectTypeDescriptorManager(QnCommonModule* commonModule)
    : base_type(commonModule),
      m_objectTypeDescriptorContainer(makeContainer<ObjectTypeDescriptorContainer>(
          commonModule, kObjectTypeDescriptorsProperty)),
      m_engineDescriptorContainer(
          makeContainer<EngineDescriptorContainer>(commonModule, kEngineDescriptorsProperty)),
      m_groupDescriptorContainer(
          makeContainer<GroupDescriptorContainer>(commonModule, kGroupDescriptorsProperty)),
      m_deviceDescriptorContainer(
          makeContainer<DeviceDescriptorContainer>(commonModule, kDeviceDescriptorsProperty))
{
}

std::optional<ObjectTypeDescriptor> ObjectTypeDescriptorManager::descriptor(
    const ObjectTypeId& id) const
{
    return fetchDescriptor(m_objectTypeDescriptorContainer, id);
}

ObjectTypeDescriptorMap ObjectTypeDescriptorManager::descriptors(
    const std::set<ObjectTypeId>& objectTypeIds) const
{
    return fetchDescriptors(
        m_objectTypeDescriptorContainer, objectTypeIds, kObjectTypeDescriptorTypeName);
}

ScopedObjectTypeIds ObjectTypeDescriptorManager::supportedObjectTypeIds(
    const QnVirtualCameraResourcePtr& device) const
{
    ScopedObjectTypeIds result;
    auto descriptor = m_deviceDescriptorContainer.mergedDescriptors(device->getId());
    if (!descriptor)
        return {};

    const auto& objectTypeIdsByEngine = descriptor->supportedObjectTypeIds;
    for (const auto& [engineId, descriptorIds]: objectTypeIdsByEngine)
    {
        for (const auto& descriptorId: descriptorIds)
        {
            const auto groupId = objectTypeGroupForEngine(engineId, descriptorId);
            result[engineId][groupId].insert(descriptorId);
        }
    }

    return result;
}

ScopedObjectTypeIds ObjectTypeDescriptorManager::supportedObjectTypeIdsUnion(
    const QnVirtualCameraResourceList& devices) const
{
    ScopedObjectTypeIds result;
    for (const auto& device: devices)
    {
        auto deviceObjectTypeIds = supportedObjectTypeIds(device);
        MapHelper::merge(&result, deviceObjectTypeIds, mergeObjectTypeIds);
    }

    return result;
}

ScopedObjectTypeIds ObjectTypeDescriptorManager::supportedObjectTypeIdsIntersection(
    const QnVirtualCameraResourceList& devices) const
{
    if (devices.isEmpty())
        return {};

    ScopedObjectTypeIds result = supportedObjectTypeIds(devices[0]);
    for (auto i = 0; i < devices.size(); ++i)
    {
        const auto deviceObjectTypeIds = supportedObjectTypeIds(devices[i]);
        MapHelper::intersected(&result, deviceObjectTypeIds, intersectObjectTypeIds);
    }

    return result;
}

ScopedObjectTypeIds ObjectTypeDescriptorManager::compatibleObjectTypeIds(
    const QnVirtualCameraResourcePtr& device) const
{
    const auto descriptor = m_deviceDescriptorContainer.mergedDescriptors(device->getId());
    if (!descriptor)
        return {};

    const auto& compatibleEngineIds = descriptor->compatibleEngineIds;
    if (compatibleEngineIds.empty())
        return {};

    ScopedObjectTypeIds result;
    for (const auto& engineId: compatibleEngineIds)
    {
        const auto engine =
            commonModule()
                ->resourcePool()
                ->getResourceById<nx::vms::common::AnalyticsEngineResource>(engineId);

        if (!engine)
        {
            NX_WARNING(this, "Analytics Engine resource with id %1 is not found", engineId);
            continue;
        }

        const auto eventTypeDescriptors = engine->analyticsObjectTypeDescriptors();
        for (const auto& [objectTypeId, eventTypeDescriptor] : eventTypeDescriptors)
        {
            for (const auto& scope: eventTypeDescriptor.scopes)
                result[scope.engineId][scope.groupId].insert(objectTypeId);
        }
    }

    auto supported = supportedObjectTypeIds(device);
    MapHelper::merge(&result, supported, mergeObjectTypeIds);

    return result;
}

ScopedObjectTypeIds ObjectTypeDescriptorManager::compatibleObjectTypeIdsUnion(
    const QnVirtualCameraResourceList& devices) const
{
    ScopedObjectTypeIds result;
    for (const auto& device : devices)
    {
        auto deviceObjectTypeIds = compatibleObjectTypeIds(device);
        MapHelper::merge(&result, deviceObjectTypeIds, mergeObjectTypeIds);
    }

    return result;
}

ScopedObjectTypeIds ObjectTypeDescriptorManager::compatibleObjectTypeIdsIntersection(
    const QnVirtualCameraResourceList& devices) const
{
    if (devices.isEmpty())
        return {};

    ScopedObjectTypeIds result = compatibleObjectTypeIds(devices[0]);
    for (auto i = 0; i < devices.size(); ++i)
    {
        const auto deviceObjectTypeIds = compatibleObjectTypeIds(devices[i]);
        MapHelper::intersected(&result, deviceObjectTypeIds, intersectObjectTypeIds);
    }

    return result;
}

void ObjectTypeDescriptorManager::updateFromEngineManifest(
    const PluginId& pluginId,
    const EngineId& engineId,
    const QString& engineName,
    const EngineManifest& manifest)
{
    m_objectTypeDescriptorContainer.mergeWithDescriptors(
        fromManifestItemListToDescriptorMap<ObjectTypeDescriptor>(engineId, manifest.objectTypes));
}

void ObjectTypeDescriptorManager::updateFromDeviceAgentManifest(
    const DeviceId& deviceId,
    const EngineId& engineId,
    const DeviceAgentManifest& manifest)
{
    m_objectTypeDescriptorContainer.mergeWithDescriptors(
        fromManifestItemListToDescriptorMap<ObjectTypeDescriptor>(engineId, manifest.objectTypes));
}

GroupId ObjectTypeDescriptorManager::objectTypeGroupForEngine(
    const EngineId& engineId,
    const ObjectTypeId& objectTypeId) const
{
    const auto descriptor = m_objectTypeDescriptorContainer.mergedDescriptors(objectTypeId);
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
