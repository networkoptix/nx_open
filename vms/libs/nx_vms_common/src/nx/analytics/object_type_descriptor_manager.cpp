// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "object_type_descriptor_manager.h"

#include <nx/analytics/properties.h>
#include <nx/analytics/utils.h>
#include <nx/analytics/helpers.h>

#include <common/common_module.h>
#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>

#include <nx/vms/common/resource/analytics_engine_resource.h>

#include <nx/utils/data_structures/map_helper.h>

namespace nx::analytics {

using namespace nx::vms::api::analytics;
using namespace nx::utils::data_structures;

ObjectTypeDescriptorManager::ObjectTypeDescriptorManager(QObject* parent) :
    base_type(parent),
    QnCommonModuleAware(parent)
{
}

std::optional<ObjectTypeDescriptor> ObjectTypeDescriptorManager::descriptor(
    const ObjectTypeId& id) const
{
    return fetchDescriptor<ObjectTypeDescriptor>(commonModule()->descriptorContainer(), id);
}

ObjectTypeDescriptorMap ObjectTypeDescriptorManager::descriptors(
    const std::set<ObjectTypeId>& objectTypeIds) const
{
    return fetchDescriptors<ObjectTypeDescriptor>(
        commonModule()->descriptorContainer(), objectTypeIds);
}

ScopedObjectTypeIds ObjectTypeDescriptorManager::supportedObjectTypeIds(
    const QnVirtualCameraResourcePtr& device) const
{
    ScopedObjectTypeIds result;

    const auto& objectTypeIdsByEngine = device->supportedObjectTypes();
    for (const auto& [engineId, objectTypeIds]: objectTypeIdsByEngine)
    {
        for (const auto& objectTypeId: objectTypeIds)
        {
            const auto groupId = objectTypeGroupForEngine(engineId, objectTypeId);
            result[engineId][groupId].insert(objectTypeId);
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
        MapHelper::merge(&result, deviceObjectTypeIds, mergeEntityIds);
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
        MapHelper::intersected(&result, deviceObjectTypeIds, intersectEntityIds);
    }

    return result;
}

ScopedObjectTypeIds ObjectTypeDescriptorManager::compatibleObjectTypeIds(
    const QnVirtualCameraResourcePtr& device) const
{
    const auto& compatibleEngines = device->compatibleAnalyticsEngineResources();
    if (compatibleEngines.empty())
        return {};

    ScopedObjectTypeIds result;
    for (const auto& engine: compatibleEngines)
    {
        const bool engineIsEnabled =
            device->enabledAnalyticsEngines().contains(engine->getId());

        if (engineIsEnabled || engine->isDeviceDependent())
        {
            auto supported = supportedObjectTypeIds(device);
            MapHelper::merge(&result, supported, mergeEntityIds);
        }
        else
        {
            const auto objectTypeDescriptors = engine->analyticsObjectTypeDescriptors();
            for (const auto& [objectTypeId, objectTypeDescriptor]: objectTypeDescriptors)
            {
                for (const auto& scope: objectTypeDescriptor.scopes)
                    result[scope.engineId][scope.groupId].insert(objectTypeId);
            }
        }
    }

    return result;
}

ScopedObjectTypeIds ObjectTypeDescriptorManager::compatibleObjectTypeIdsUnion(
    const QnVirtualCameraResourceList& devices) const
{
    ScopedObjectTypeIds result;
    for (const auto& device : devices)
    {
        auto deviceObjectTypeIds = compatibleObjectTypeIds(device);
        MapHelper::merge(&result, deviceObjectTypeIds, mergeEntityIds);
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
        MapHelper::intersected(&result, deviceObjectTypeIds, intersectEntityIds);
    }

    return result;
}

GroupId ObjectTypeDescriptorManager::objectTypeGroupForEngine(
    const EngineId& engineId,
    const ObjectTypeId& objectTypeId) const
{
    const auto descriptor = fetchDescriptor<ObjectTypeDescriptor>(
        commonModule()->descriptorContainer(), objectTypeId);

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
