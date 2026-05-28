// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "state_helper.h"

#include <core/resource/camera_resource.h>
#include <core/resource/resource_fwd.h>
#include <nx/vms/common/resource/analytics_engine_resource.h>

namespace nx::analytics::taxonomy {

using nx::vms::common::AnalyticsEngineResourcePtr;

template<typename EntityType>
using SupportedEntityTypesFetcher =
    std::function<std::map<std::string, EntityType*>(const QnVirtualCameraResourcePtr&)>;

using SupportedEntityTypeIdsFetcher =
    std::function<std::map<nx::Uuid, std::set<std::string>>(const QnVirtualCameraResourcePtr&)>;

template<typename EntityType>
using EntityTypeFetcher = std::function<EntityType*(const std::string&)>;

using EngineEntityTypeIdsFetcher =
    std::function<std::set<std::string>(const AnalyticsEngineResourcePtr&)>;

//--------------------------------------------------------------------------------------------------

template<typename Entity>
bool operator<(const ScopedEntity<Entity>& first, const ScopedEntity<Entity>& second)
{
    if (first.engine != second.engine)
        return first.engine < second.engine;
    if (first.group != second.group)
        return first.group < second.group;

    return first.entityType < second.entityType;
}

template<typename EntityType>
void intersect(
    std::map<std::string, EntityType*>* inOutFirst,
    const std::map<std::string, EntityType*>& second)
{
    for (auto it = inOutFirst->begin(); it != inOutFirst->end();)
    {
        if (second.find(it->first) == second.cend())
            it = inOutFirst->erase(it);
        else
            ++it;
    }
}

//--------------------------------------------------------------------------------------------------

template<typename EntityType>
std::map<std::string, EntityType*> supportedEntityTypes(
    const QnVirtualCameraResourcePtr& device,
    SupportedEntityTypeIdsFetcher supportedEntityTypeIdsFetcher,
    EntityTypeFetcher<EntityType> entityTypeFetcher)
{
    std::map<std::string, EntityType*> result;

    const auto entityTypeIdsByEngine = supportedEntityTypeIdsFetcher(device);

    for (const auto& [engineId, entityTypeIds]: entityTypeIdsByEngine)
    {
        for (const std::string& entityTypeId: entityTypeIds)
        {
            if (EntityType* const entityType = entityTypeFetcher(entityTypeId))
                result.emplace(entityTypeId, entityType);
        }
    }

    return result;
}

template<typename EntityType>
std::set<ScopedEntity<EntityType>> supportedScopedEntityTypes(
    const QnVirtualCameraResourcePtr& device,
    const std::shared_ptr<AbstractState>& taxonomyState,
    SupportedEntityTypeIdsFetcher supportedEntityTypeIdsFetcher,
    EntityTypeFetcher<EntityType> entityTypeFetcher)
{
    std::set<ScopedEntity<EntityType>> result;

    for (const auto& [engineId, entityTypeIds]: supportedEntityTypeIdsFetcher(device))
    {
        ScopedEntity<EntityType> scopedEntityType;
        scopedEntityType.engine = taxonomyState->engineById(engineId.toStdString(QUuid::WithBraces));
        if (!scopedEntityType.engine)
            continue;

        for (const std::string& entityTypeId: entityTypeIds)
        {
            EntityType* const entityType = entityTypeFetcher(entityTypeId);
            if (!entityType)
                continue;

            scopedEntityType.entityType = entityType;
            for (const auto& scope: entityType->scopes())
            {
                if (scope.engine() == scopedEntityType.engine)
                {
                    scopedEntityType.group = scope.group();
                    break;
                }
            }

            result.insert(scopedEntityType);
        }
    }

    return result;
}

template<typename EntityType>
std::vector<EngineScope<EntityType>> supportedEntityTypeTreeUnion(
    const QnVirtualCameraResourceList& devices,
    const std::shared_ptr<AbstractState>& taxonomyState,
    const std::vector<ScopedEntity<EntityType>>& additionalEntities,
    SupportedEntityTypeIdsFetcher supportedEntityTypeIdsFetcher,
    EntityTypeFetcher<EntityType> entityTypeFetcher)
{
    std::set<ScopedEntity<EntityType>> scopedEntityTypes;
    for (const QnVirtualCameraResourcePtr& device: devices)
    {
        std::set<ScopedEntity<EntityType>> deviceScopedEntityTypes =
            supportedScopedEntityTypes<EntityType>(
                device,
                taxonomyState,
                supportedEntityTypeIdsFetcher,
                entityTypeFetcher);

        scopedEntityTypes.insert(deviceScopedEntityTypes.begin(), deviceScopedEntityTypes.end());
    }

    scopedEntityTypes.insert(additionalEntities.begin(), additionalEntities.end());
    return entityTypeTree(scopedEntityTypes);
}

template<typename EntityType>
std::vector<EngineScope<EntityType>> supportedEntityTypeTreeIntersection(
    const QnVirtualCameraResourceList& devices,
    const std::shared_ptr<AbstractState>& taxonomyState,
    const std::vector<ScopedEntity<EntityType>>& additionalEntities,
    SupportedEntityTypeIdsFetcher supportedEntityTypeIdsFetcher,
    EntityTypeFetcher<EntityType> entityTypeFetcher)
{
    if (devices.empty())
        return {};

    std::set<ScopedEntity<EntityType>> intersection;
    std::set<ScopedEntity<EntityType>> deviceScopedEntityTypes =
        supportedScopedEntityTypes<EntityType>(
            devices[0],
            taxonomyState,
            supportedEntityTypeIdsFetcher,
            entityTypeFetcher);

    for (int i = 1; i < devices.size(); ++i)
    {
        std::set<ScopedEntity<EntityType>> otherDeviceScopedEntityTypes =
            supportedScopedEntityTypes<EntityType>(
                devices[i],
                taxonomyState,
                supportedEntityTypeIdsFetcher,
                entityTypeFetcher);

        std::set_intersection(
            deviceScopedEntityTypes.begin(),
            deviceScopedEntityTypes.end(),
            otherDeviceScopedEntityTypes.begin(),
            otherDeviceScopedEntityTypes.end(),
            std::inserter(intersection, intersection.begin()));

        deviceScopedEntityTypes = std::move(intersection);
        intersection.clear();

        if (deviceScopedEntityTypes.empty())
            break;
    }

    deviceScopedEntityTypes.insert(additionalEntities.begin(), additionalEntities.end());
    return entityTypeTree(deviceScopedEntityTypes);
}

//--------------------------------------------------------------------------------------------------

template<typename EntityType>
std::map<std::string, EntityType*> compatibleEntityTypes(
    const QnVirtualCameraResourcePtr& device,
    SupportedEntityTypesFetcher<EntityType> supportedEntityTypesFetcher,
    EngineEntityTypeIdsFetcher engineEntityTypeIdsFetcher,
    EntityTypeFetcher<EntityType> entityTypeFetcher)
{
    const auto& compatibleEngines = device->compatibleAnalyticsEngineResources();
    if (compatibleEngines.empty())
        return {};

    std::map<std::string, EntityType*> result;
    for (const auto& engine: compatibleEngines)
    {
        const bool engineIsEnabled = device->enabledAnalyticsEngines().contains(engine->getId());
        if (engineIsEnabled || engine->isDeviceDependent())
        {
            result.merge(supportedEntityTypesFetcher(device));
        }
        else
        {
            const std::set<std::string> entityTypeIds = engineEntityTypeIdsFetcher(engine);
            for (const std::string& entityTypeId: entityTypeIds)
            {
                EntityType* entityType = entityTypeFetcher(entityTypeId);
                if (entityType)
                    result.emplace(entityTypeId, entityType);
            }
        }
    }

    return result;
}

template<typename EntityType>
std::set<ScopedEntity<EntityType>> compatibleScopedEntityTypes(
    const QnVirtualCameraResourcePtr& device,
    const std::shared_ptr<AbstractState>& taxonomyState,
    SupportedEntityTypeIdsFetcher supportedEntityTypeIdsFetcher,
    EngineEntityTypeIdsFetcher engineEntityTypeIdsFetcher,
    EntityTypeFetcher<EntityType> entityTypeFetcher)
{
    std::set<ScopedEntity<EntityType>> result;
    for (const auto& engine: device->compatibleAnalyticsEngineResources())
    {
        ScopedEntity<EntityType> scopedEntityType;
        scopedEntityType.engine =
            taxonomyState->engineById(engine->getId().toStdString(QUuid::WithBraces));
        if (!scopedEntityType.engine)
            continue;

        const bool engineIsEnabled = device->enabledAnalyticsEngines().contains(engine->getId());
        auto supportedEntityTypesByEngine = supportedEntityTypeIdsFetcher(device);

        const std::set<std::string> entityTypeIds = engineIsEnabled || engine->isDeviceDependent()
            ? supportedEntityTypesByEngine[engine->getId()]
            : engineEntityTypeIdsFetcher(engine);

        if (entityTypeIds.empty())
            continue;

        for (const std::string& entityTypeId: entityTypeIds)
        {
            EntityType* entityType = entityTypeFetcher(entityTypeId);
            if (!entityType)
                continue;

            scopedEntityType.entityType = entityType;

            const std::string engineId = engine->getId().toStdString(QUuid::WithBraces);
            for (const auto& scope: entityType->scopes())
            {
                const AbstractEngine* taxonomyEngine = scope.engine();
                if (taxonomyEngine && taxonomyEngine->id() == engineId)
                {
                    scopedEntityType.group = scope.group();
                    break;
                }
            }

            result.insert(scopedEntityType);
        }
    }

    return result;
}

template<typename EntityType>
std::vector<EngineScope<EntityType>> compatibleEntityTypeTreeUnion(
    const QnVirtualCameraResourceList& devices,
    const std::shared_ptr<AbstractState>& taxonomyState,
    const std::vector<ScopedEntity<EntityType>>& additionalEntities,
    SupportedEntityTypeIdsFetcher supportedEntityTypeIdsFetcher,
    EngineEntityTypeIdsFetcher engineEntityTypeIdsFetcher,
    EntityTypeFetcher<EntityType> entityTypeFetcher)
{
    std::set<ScopedEntity<EntityType>> scopedEntityTypes;
    for (const QnVirtualCameraResourcePtr& device: devices)
    {
        std::set<ScopedEntity<EntityType>> deviceScopedEntityTypes =
            compatibleScopedEntityTypes<EntityType>(
                device,
                taxonomyState,
                supportedEntityTypeIdsFetcher,
                engineEntityTypeIdsFetcher,
                entityTypeFetcher);

        scopedEntityTypes.insert(deviceScopedEntityTypes.begin(), deviceScopedEntityTypes.end());
    }

    scopedEntityTypes.insert(additionalEntities.begin(), additionalEntities.end());
    return entityTypeTree(scopedEntityTypes);
}

template<typename EntityType>
std::vector<EngineScope<EntityType>> compatibleEntityTypeTreeIntersection(
    const QnVirtualCameraResourceList& devices,
    const std::shared_ptr<AbstractState>& taxonomyState,
    const std::vector<ScopedEntity<EntityType>>& additionalEntities,
    SupportedEntityTypeIdsFetcher supportedEntityTypeIdsFetcher,
    EngineEntityTypeIdsFetcher engineEntityTypeIdsFetcher,
    EntityTypeFetcher<EntityType> entityTypeFetcher)
{
    if (devices.empty())
        return {};

    std::set<ScopedEntity<EntityType>> intersection;
    std::set<ScopedEntity<EntityType>> deviceScopedEntityTypes =
        compatibleScopedEntityTypes<EntityType>(
            devices[0],
            taxonomyState,
            supportedEntityTypeIdsFetcher,
            engineEntityTypeIdsFetcher,
            entityTypeFetcher);

    for (int i = 1; i < devices.size(); ++i)
    {
        std::set<ScopedEntity<EntityType>> otherDeviceScopedEntityTypes =
            compatibleScopedEntityTypes<EntityType>(
                devices[i],
                taxonomyState,
                supportedEntityTypeIdsFetcher,
                engineEntityTypeIdsFetcher,
                entityTypeFetcher);

        std::set_intersection(
            deviceScopedEntityTypes.begin(),
            deviceScopedEntityTypes.end(),
            otherDeviceScopedEntityTypes.begin(),
            otherDeviceScopedEntityTypes.end(),
            std::inserter(intersection, intersection.begin()));

        deviceScopedEntityTypes = std::move(intersection);
        intersection.clear();

        if (deviceScopedEntityTypes.empty())
            break;
    }

    deviceScopedEntityTypes.insert(additionalEntities.begin(), additionalEntities.end());
    return entityTypeTree(deviceScopedEntityTypes);
}

//--------------------------------------------------------------------------------------------------

template<typename EntityType>
void sortEntityTypeTree(std::vector<EngineScope<EntityType>>* inOutEntityTypeTree)
{
    for (EngineScope<EntityType>& engineScope: *inOutEntityTypeTree)
    {
        for (GroupScope<EntityType>& groupScope: engineScope.groups)
        {
            std::ranges::sort(
                groupScope.entities,
                [](const EntityType* first, const EntityType* second)
                {
                    return first->name() < second->name();
                });
        }

        std::ranges::sort(
            engineScope.groups,
            [](const GroupScope<EntityType>& first, GroupScope<EntityType>& second)
            {
                if (!first.group)
                    return second.group != nullptr;

                if (!second.group)
                    return false;

                return first.group->name() < second.group->name();
            });
    }

    std::ranges::sort(
        *inOutEntityTypeTree,
        [](const EngineScope<EntityType>& first, EngineScope<EntityType>& second)
        {
            if (!first.engine)
                return second.engine != nullptr;

            if (!second.engine)
                return false;

            return first.engine->name() < second.engine->name();
        });
}

template<typename EntityType>
std::vector<EngineScope<EntityType>> entityTypeTree(
    const std::set<ScopedEntity<EntityType>>& scopedEntities)
{
    std::vector<EngineScope<EntityType>> result;

    EngineScope<EntityType> currentEngineScope;
    GroupScope<EntityType> currentGroupScope;

    bool groupIsInitialized = false;
    for (const auto& scopedEntity: scopedEntities)
    {
        if (currentEngineScope.engine && (scopedEntity.engine != currentEngineScope.engine))
        {
            if (groupIsInitialized)
            {
                currentEngineScope.groups.push_back(std::move(currentGroupScope));
                currentGroupScope = {};
            }

            result.push_back(std::move(currentEngineScope));
            currentEngineScope = {};
            groupIsInitialized = false;
        }

        currentEngineScope.engine = scopedEntity.engine;
        if (groupIsInitialized && (scopedEntity.group != currentGroupScope.group))
        {
            currentEngineScope.groups.push_back(std::move(currentGroupScope));
            currentGroupScope = {};
        }

        currentGroupScope.group = scopedEntity.group;
        groupIsInitialized = true;
        currentGroupScope.entities.push_back(scopedEntity.entityType);
    }

    if (currentEngineScope.engine)
    {
        if (groupIsInitialized && !currentGroupScope.entities.empty())
            currentEngineScope.groups.push_back(std::move(currentGroupScope));

        result.push_back(std::move(currentEngineScope));
    }

    sortEntityTypeTree(&result);
    return result;
}

//--------------------------------------------------------------------------------------------------

StateHelper::StateHelper(std::shared_ptr<AbstractState> state):
    m_state(std::move(state))
{
    NX_ASSERT(m_state);
}

std::map<std::string, EventType*> StateHelper::supportedEventTypes(
    const QnVirtualCameraResourcePtr& device) const
{
    return supportedEntityTypes<EventType>(
        device,
        [](const QnVirtualCameraResourcePtr& device) { return device->supportedEventTypes(); },
        [this](const std::string& eventTypeId) { return m_state->eventTypeById(eventTypeId); });
}

std::map<std::string, EventType*> StateHelper::supportedEventTypeUnion(
    const QnVirtualCameraResourceList& devices) const
{
    std::map<std::string, EventType*> result;
    for (const QnVirtualCameraResourcePtr& device: devices)
        result.merge(supportedEventTypes(device));

    return result;
}

std::map<std::string, EventType*> StateHelper::supportedEventTypeIntersection(
    const QnVirtualCameraResourceList& devices) const
{
    if (devices.empty())
        return {};

    std::map<std::string, EventType*> result = supportedEventTypes(devices[0]);
    for (int i = 1; i < devices.size(); ++i)
        intersect(&result, supportedEventTypes(devices[i]));

    return result;
}

std::vector<EngineScope<EventType>> StateHelper::supportedEventTypeTreeUnion(
    const QnVirtualCameraResourceList& devices,
    const std::vector<ScopedEntity<EventType>>& additionalEntities) const
{
    return supportedEntityTypeTreeUnion<EventType>(
        devices,
        m_state,
        additionalEntities,
        [&](const QnVirtualCameraResourcePtr& device) { return device->supportedEventTypes(); },
        [this](const std::string& eventTypeId) { return m_state->eventTypeById(eventTypeId); });
}

std::vector<EngineScope<EventType>> StateHelper::supportedEventTypeTreeIntersection(
    const QnVirtualCameraResourceList& devices,
    const std::vector<ScopedEntity<EventType>>& additionalEntities) const
{
    return supportedEntityTypeTreeIntersection<EventType>(
        devices,
        m_state,
        additionalEntities,
        [&](const QnVirtualCameraResourcePtr& device) { return device->supportedEventTypes(); },
        [this](const std::string& eventTypeId) { return m_state->eventTypeById(eventTypeId); });
}

//--------------------------------------------------------------------------------------------------

std::map<std::string, EventType*> StateHelper::compatibleEventTypes(
    const QnVirtualCameraResourcePtr& device) const
{
    return compatibleEntityTypes<EventType>(
        device,
        [this](const QnVirtualCameraResourcePtr& device) { return supportedEventTypes(device); },
        [](const AnalyticsEngineResourcePtr& engine) { return engine->eventTypeIds(); },
        [this](const std::string& objectTypeId) { return m_state->eventTypeById(objectTypeId); });
}

std::map<std::string, EventType*> StateHelper::compatibleEventTypeUnion(
    const QnVirtualCameraResourceList& devices) const
{
    std::map<std::string, EventType*> result;
    for (const QnVirtualCameraResourcePtr& device: devices)
        result.merge(compatibleEventTypes(device));

    return result;
}

std::map<std::string, EventType*> StateHelper::compatibleEventTypeIntersection(
    const QnVirtualCameraResourceList& devices) const
{
    if (devices.empty())
        return {};

    std::map<std::string, EventType*> result = compatibleEventTypes(devices[0]);
    for (int i = 1; i < devices.size(); ++i)
        intersect(&result, compatibleEventTypes(devices[i]));

    return result;
}

std::vector<EngineScope<EventType>> StateHelper::compatibleEventTypeTreeUnion(
    const QnVirtualCameraResourceList& devices,
    const std::vector<ScopedEntity<EventType>>& additionalEntities) const
{
    return compatibleEntityTypeTreeUnion<EventType>(
        devices,
        m_state,
        additionalEntities,
        [](const QnVirtualCameraResourcePtr& device) { return device->supportedEventTypes(); },
        [](const AnalyticsEngineResourcePtr& engine) { return engine->eventTypeIds(); },
        [this](const std::string& eventTypeId) { return m_state->eventTypeById(eventTypeId); });
}

std::vector<EngineScope<EventType>> StateHelper::compatibleEventTypeTreeIntersection(
    const QnVirtualCameraResourceList& devices,
    const std::vector<ScopedEntity<EventType>>& additionalEntities) const
{
    return compatibleEntityTypeTreeIntersection<EventType>(
        devices,
        m_state,
        additionalEntities,
        [](const QnVirtualCameraResourcePtr& device) { return device->supportedEventTypes(); },
        [](const AnalyticsEngineResourcePtr& engine) { return engine->eventTypeIds(); },
        [this](const std::string& eventTypeId) { return m_state->eventTypeById(eventTypeId); });
}

//--------------------------------------------------------------------------------------------------

std::map<std::string, ObjectType*> StateHelper::supportedObjectTypes(
    const QnVirtualCameraResourcePtr& device) const
{
    return supportedEntityTypes<ObjectType>(
        device,
        [](const QnVirtualCameraResourcePtr& device) { return device->supportedObjectTypes(); },
        [this](const std::string& objectTypeId) { return m_state->objectTypeById(objectTypeId); });
}

std::map<std::string, ObjectType*> StateHelper::supportedObjectTypeUnion(
    const QnVirtualCameraResourceList& devices) const
{
    std::map<std::string, ObjectType*> result;
    for (const QnVirtualCameraResourcePtr& device: devices)
        result.merge(supportedObjectTypes(device));

    return result;
}

std::map<std::string, ObjectType*> StateHelper::supportedObjectTypeIntersection(
    const QnVirtualCameraResourceList& devices) const
{
    if (devices.empty())
        return {};

    std::map<std::string, ObjectType*> result = supportedObjectTypes(devices[0]);
    for (int i = 1; i < devices.size(); ++i)
        intersect(&result, supportedObjectTypes(devices[i]));

    return result;
}

std::vector<EngineScope<ObjectType>> StateHelper::supportedObjectTypeTreeUnion(
    const QnVirtualCameraResourceList& devices,
    const std::vector<ScopedEntity<ObjectType>>& additionalEntities) const
{
    return supportedEntityTypeTreeUnion<ObjectType>(
        devices,
        m_state,
        additionalEntities,
        [](const QnVirtualCameraResourcePtr& device) { return device->supportedObjectTypes(); },
        [this](const std::string& objectTypeId) { return m_state->objectTypeById(objectTypeId); });
}

std::vector<EngineScope<ObjectType>> StateHelper::supportedObjectTypeTreeIntersection(
    const QnVirtualCameraResourceList& devices,
    const std::vector<ScopedEntity<ObjectType>>& additionalEntities) const
{
    return supportedEntityTypeTreeIntersection<ObjectType>(
        devices,
        m_state,
        additionalEntities,
        [](const QnVirtualCameraResourcePtr& device) { return device->supportedObjectTypes(); },
        [this](const std::string& objectTypeId) { return m_state->objectTypeById(objectTypeId); });
}

//--------------------------------------------------------------------------------------------------

std::map<std::string, ObjectType*> StateHelper::compatibleObjectTypes(
    const QnVirtualCameraResourcePtr& device) const
{
    return compatibleEntityTypes<ObjectType>(
        device,
        [this](const QnVirtualCameraResourcePtr& device) { return supportedObjectTypes(device); },
        [](const AnalyticsEngineResourcePtr& engine) { return engine->objectTypeIds(); },
        [this](const std::string& objectTypeId) { return m_state->objectTypeById(objectTypeId); });
}

std::map<std::string, ObjectType*> StateHelper::compatibleObjectTypeUnion(
    const QnVirtualCameraResourceList& devices) const
{
    std::map<std::string, ObjectType*> result;
    for (const QnVirtualCameraResourcePtr& device: devices)
        result.merge(compatibleObjectTypes(device));

    return result;
}

std::map<std::string, ObjectType*> StateHelper::compatibleObjectTypeIntersection(
    const QnVirtualCameraResourceList& devices) const
{
    if (devices.empty())
        return {};

    std::map<std::string, ObjectType*> result = compatibleObjectTypes(devices[0]);
    for (int i = 1; i < devices.size(); ++i)
        intersect(&result, compatibleObjectTypes(devices[i]));

    return result;
}

std::vector<EngineScope<ObjectType>> StateHelper::compatibleObjectTypeTreeUnion(
    const QnVirtualCameraResourceList& devices,
    const std::vector<ScopedEntity<ObjectType>>& additionalEntities) const
{
    return compatibleEntityTypeTreeUnion<ObjectType>(
        devices,
        m_state,
        additionalEntities,
        [](const QnVirtualCameraResourcePtr& device) { return device->supportedObjectTypes(); },
        [](const AnalyticsEngineResourcePtr& engine) { return engine->objectTypeIds(); },
        [this](const std::string& objectTypeId) { return m_state->objectTypeById(objectTypeId); });
}

std::vector<EngineScope<ObjectType>> StateHelper::compatibleObjectTypeTreeIntersection(
    const QnVirtualCameraResourceList& devices,
    const std::vector<ScopedEntity<ObjectType>>& additionalEntities) const
{
    return compatibleEntityTypeTreeIntersection<ObjectType>(
        devices,
        m_state,
        additionalEntities,
        [](const QnVirtualCameraResourcePtr& device) { return device->supportedObjectTypes(); },
        [](const AnalyticsEngineResourcePtr& engine) { return engine->objectTypeIds(); },
        [this](const std::string& objectTypeId) { return m_state->objectTypeById(objectTypeId); });
}

} // namespace nx::analytics::taxonomy
