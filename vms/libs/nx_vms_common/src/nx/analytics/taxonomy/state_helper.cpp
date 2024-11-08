// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "state_helper.h"

#include <core/resource/camera_resource.h>
#include <core/resource/resource_fwd.h>
#include <nx/vms/common/resource/analytics_engine_resource.h>

namespace nx::analytics::taxonomy {

using nx::vms::common::AnalyticsEngineResourcePtr;

template<typename EntityType>
using SupportedEntityTypesFetcher =
    std::function<std::map<QString, EntityType*>(const QnVirtualCameraResourcePtr&)>;

using SupportedEntityTypeIdsFetcher =
    std::function<std::map<nx::Uuid, std::set<QString>>(const QnVirtualCameraResourcePtr&)>;

template<typename EntityType>
using EntityTypeFetcher = std::function<EntityType*(const QString&)>;

using EngineEntityTypeIdsFetcher =
    std::function<std::set<QString>(const AnalyticsEngineResourcePtr&)>;

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
    std::map<QString, EntityType*>* inOutFirst,
    const std::map<QString, EntityType*>& second)
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
std::map<QString, EntityType*> supportedEntityTypes(
    const QnVirtualCameraResourcePtr& device,
    SupportedEntityTypeIdsFetcher supportedEntityTypeIdsFetcher,
    EntityTypeFetcher<EntityType> entityTypeFetcher)
{
    std::map<QString, EntityType*> result;

    const auto entityTypeIdsByEngine = supportedEntityTypeIdsFetcher(device);

    for (const auto& [engineId, entityTypeIds]: entityTypeIdsByEngine)
    {
        for (const QString& entityTypeId: entityTypeIds)
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
        scopedEntityType.engine = taxonomyState->engineById(engineId.toString());
        if (!scopedEntityType.engine)
            continue;

        for (const QString& entityTypeId: entityTypeIds)
        {
            EntityType* const entityType = entityTypeFetcher(entityTypeId);
            if (!entityType)
                continue;

            scopedEntityType.entityType = entityType;
            for (const AbstractScope* scope: entityType->scopes())
            {
                if (scope->engine() == scopedEntityType.engine)
                {
                    scopedEntityType.group = scope->group();
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
std::map<QString, EntityType*> compatibleEntityTypes(
    const QnVirtualCameraResourcePtr& device,
    SupportedEntityTypesFetcher<EntityType> supportedEntityTypesFetcher,
    EngineEntityTypeIdsFetcher engineEntityTypeIdsFetcher,
    EntityTypeFetcher<EntityType> entityTypeFetcher)
{
    const auto& compatibleEngines = device->compatibleAnalyticsEngineResources();
    if (compatibleEngines.empty())
        return {};

    std::map<QString, EntityType*> result;
    for (const auto& engine: compatibleEngines)
    {
        const bool engineIsEnabled = device->enabledAnalyticsEngines().contains(engine->getId());
        if (engineIsEnabled || engine->isDeviceDependent())
        {
            result.merge(supportedEntityTypesFetcher(device));
        }
        else
        {
            const std::set<QString> entityTypeIds = engineEntityTypeIdsFetcher(engine);
            for (const QString& entityTypeId: entityTypeIds)
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
        scopedEntityType.engine = taxonomyState->engineById(engine->getId().toString());
        if (!scopedEntityType.engine)
            continue;

        const bool engineIsEnabled = device->enabledAnalyticsEngines().contains(engine->getId());
        auto supportedEntityTypesByEngine = supportedEntityTypeIdsFetcher(device);

        const std::set<QString> entityTypeIds = engineIsEnabled || engine->isDeviceDependent()
            ? supportedEntityTypesByEngine[engine->getId()]
            : engineEntityTypeIdsFetcher(engine);

        if (entityTypeIds.empty())
            continue;

        for (const QString& entityTypeId: entityTypeIds)
        {
            EntityType* entityType = entityTypeFetcher(entityTypeId);
            if (!entityType)
                continue;

            scopedEntityType.entityType = entityType;

            const QString engineId = engine->getId().toString();
            for (const AbstractScope* scope: entityType->scopes())
            {
                const AbstractEngine* taxonomyEngine = scope->engine();
                if (taxonomyEngine && taxonomyEngine->id() == engineId)
                {
                    scopedEntityType.group = scope->group();
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
            std::sort(groupScope.entities.begin(),
                groupScope.entities.end(),
                [](const EntityType* first, const EntityType* second)
                {
                    return first->name() < second->name();
                });
        }

        std::sort(engineScope.groups.begin(),
            engineScope.groups.end(),
            [](const GroupScope<EntityType>& first, GroupScope<EntityType>& second)
            {
                if (!first.group)
                    return second.group != nullptr;

                if (!second.group)
                    return false;

                return first.group->name() < second.group->name();
            });
    }

    std::sort(inOutEntityTypeTree->begin(), inOutEntityTypeTree->end(),
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

std::map<QString, AbstractEventType*> StateHelper::supportedEventTypes(
    const QnVirtualCameraResourcePtr& device) const
{
    return supportedEntityTypes<AbstractEventType>(
        device,
        [](const QnVirtualCameraResourcePtr& device) { return device->supportedEventTypes(); },
        [this](const QString& eventTypeId) { return m_state->eventTypeById(eventTypeId); });
}

std::map<QString, AbstractEventType*> StateHelper::supportedEventTypeUnion(
    const QnVirtualCameraResourceList& devices) const
{
    std::map<QString, AbstractEventType*> result;
    for (const QnVirtualCameraResourcePtr& device: devices)
        result.merge(supportedEventTypes(device));

    return result;
}

std::map<QString, AbstractEventType*> StateHelper::supportedEventTypeIntersection(
    const QnVirtualCameraResourceList& devices) const
{
    if (devices.empty())
        return {};

    std::map<QString, AbstractEventType*> result = supportedEventTypes(devices[0]);
    for (int i = 1; i < devices.size(); ++i)
        intersect(&result, supportedEventTypes(devices[i]));

    return result;
}

std::vector<EngineScope<AbstractEventType>> StateHelper::supportedEventTypeTreeUnion(
    const QnVirtualCameraResourceList& devices,
    const std::vector<ScopedEntity<AbstractEventType>>& additionalEntities) const
{
    return supportedEntityTypeTreeUnion<AbstractEventType>(
        devices,
        m_state,
        additionalEntities,
        [&](const QnVirtualCameraResourcePtr& device) { return device->supportedEventTypes(); },
        [this](const QString& eventTypeId) { return m_state->eventTypeById(eventTypeId); });
}

std::vector<EngineScope<AbstractEventType>> StateHelper::supportedEventTypeTreeIntersection(
    const QnVirtualCameraResourceList& devices,
    const std::vector<ScopedEntity<AbstractEventType>>& additionalEntities) const
{
    return supportedEntityTypeTreeIntersection<AbstractEventType>(
        devices,
        m_state,
        additionalEntities,
        [&](const QnVirtualCameraResourcePtr& device) { return device->supportedEventTypes(); },
        [this](const QString& eventTypeId) { return m_state->eventTypeById(eventTypeId); });
}

//--------------------------------------------------------------------------------------------------

std::map<QString, AbstractEventType*> StateHelper::compatibleEventTypes(
    const QnVirtualCameraResourcePtr& device) const
{
    return compatibleEntityTypes<AbstractEventType>(
        device,
        [this](const QnVirtualCameraResourcePtr& device) { return supportedEventTypes(device); },
        [](const AnalyticsEngineResourcePtr& engine) { return engine->eventTypeIds(); },
        [this](const QString& objectTypeId) { return m_state->eventTypeById(objectTypeId); });
}

std::map<QString, AbstractEventType*> StateHelper::compatibleEventTypeUnion(
    const QnVirtualCameraResourceList& devices) const
{
    std::map<QString, AbstractEventType*> result;
    for (const QnVirtualCameraResourcePtr& device: devices)
        result.merge(compatibleEventTypes(device));

    return result;
}

std::map<QString, AbstractEventType*> StateHelper::compatibleEventTypeIntersection(
    const QnVirtualCameraResourceList& devices) const
{
    if (devices.empty())
        return {};

    std::map<QString, AbstractEventType*> result = compatibleEventTypes(devices[0]);
    for (int i = 1; i < devices.size(); ++i)
        intersect(&result, compatibleEventTypes(devices[i]));

    return result;
}

std::vector<EngineScope<AbstractEventType>> StateHelper::compatibleEventTypeTreeUnion(
    const QnVirtualCameraResourceList& devices,
    const std::vector<ScopedEntity<AbstractEventType>>& additionalEntities) const
{
    return compatibleEntityTypeTreeUnion<AbstractEventType>(
        devices,
        m_state,
        additionalEntities,
        [](const QnVirtualCameraResourcePtr& device) { return device->supportedEventTypes(); },
        [](const AnalyticsEngineResourcePtr& engine) { return engine->eventTypeIds(); },
        [this](const QString& eventTypeId) { return m_state->eventTypeById(eventTypeId); });
}

std::vector<EngineScope<AbstractEventType>> StateHelper::compatibleEventTypeTreeIntersection(
    const QnVirtualCameraResourceList& devices,
    const std::vector<ScopedEntity<AbstractEventType>>& additionalEntities) const
{
    return compatibleEntityTypeTreeIntersection<AbstractEventType>(
        devices,
        m_state,
        additionalEntities,
        [](const QnVirtualCameraResourcePtr& device) { return device->supportedEventTypes(); },
        [](const AnalyticsEngineResourcePtr& engine) { return engine->eventTypeIds(); },
        [this](const QString& eventTypeId) { return m_state->eventTypeById(eventTypeId); });
}

//--------------------------------------------------------------------------------------------------

std::map<QString, AbstractObjectType*> StateHelper::supportedObjectTypes(
    const QnVirtualCameraResourcePtr& device) const
{
    return supportedEntityTypes<AbstractObjectType>(
        device,
        [](const QnVirtualCameraResourcePtr& device) { return device->supportedObjectTypes(); },
        [this](const QString& objectTypeId) { return m_state->objectTypeById(objectTypeId); });
}

std::map<QString, AbstractObjectType*> StateHelper::supportedObjectTypeUnion(
    const QnVirtualCameraResourceList& devices) const
{
    std::map<QString, AbstractObjectType*> result;
    for (const QnVirtualCameraResourcePtr& device: devices)
        result.merge(supportedObjectTypes(device));

    return result;
}

std::map<QString, AbstractObjectType*> StateHelper::supportedObjectTypeIntersection(
    const QnVirtualCameraResourceList& devices) const
{
    if (devices.empty())
        return {};

    std::map<QString, AbstractObjectType*> result = supportedObjectTypes(devices[0]);
    for (int i = 1; i < devices.size(); ++i)
        intersect(&result, supportedObjectTypes(devices[i]));

    return result;
}

std::vector<EngineScope<AbstractObjectType>> StateHelper::supportedObjectTypeTreeUnion(
    const QnVirtualCameraResourceList& devices,
    const std::vector<ScopedEntity<AbstractObjectType>>& additionalEntities) const
{
    return supportedEntityTypeTreeUnion<AbstractObjectType>(
        devices,
        m_state,
        additionalEntities,
        [](const QnVirtualCameraResourcePtr& device) { return device->supportedObjectTypes(); },
        [this](const QString& objectTypeId) { return m_state->objectTypeById(objectTypeId); });
}

std::vector<EngineScope<AbstractObjectType>> StateHelper::supportedObjectTypeTreeIntersection(
    const QnVirtualCameraResourceList& devices,
    const std::vector<ScopedEntity<AbstractObjectType>>& additionalEntities) const
{
    return supportedEntityTypeTreeIntersection<AbstractObjectType>(
        devices,
        m_state,
        additionalEntities,
        [](const QnVirtualCameraResourcePtr& device) { return device->supportedObjectTypes(); },
        [this](const QString& objectTypeId) { return m_state->objectTypeById(objectTypeId); });
}

//--------------------------------------------------------------------------------------------------

std::map<QString, AbstractObjectType*> StateHelper::compatibleObjectTypes(
    const QnVirtualCameraResourcePtr& device) const
{
    return compatibleEntityTypes<AbstractObjectType>(
        device,
        [this](const QnVirtualCameraResourcePtr& device) { return supportedObjectTypes(device); },
        [](const AnalyticsEngineResourcePtr& engine) { return engine->objectTypeIds(); },
        [this](const QString& objectTypeId) { return m_state->objectTypeById(objectTypeId); });
}

std::map<QString, AbstractObjectType*> StateHelper::compatibleObjectTypeUnion(
    const QnVirtualCameraResourceList& devices) const
{
    std::map<QString, AbstractObjectType*> result;
    for (const QnVirtualCameraResourcePtr& device: devices)
        result.merge(compatibleObjectTypes(device));

    return result;
}

std::map<QString, AbstractObjectType*> StateHelper::compatibleObjectTypeIntersection(
    const QnVirtualCameraResourceList& devices) const
{
    if (devices.empty())
        return {};

    std::map<QString, AbstractObjectType*> result = compatibleObjectTypes(devices[0]);
    for (int i = 1; i < devices.size(); ++i)
        intersect(&result, compatibleObjectTypes(devices[i]));

    return result;
}

std::vector<EngineScope<AbstractObjectType>> StateHelper::compatibleObjectTypeTreeUnion(
    const QnVirtualCameraResourceList& devices,
    const std::vector<ScopedEntity<AbstractObjectType>>& additionalEntities) const
{
    return compatibleEntityTypeTreeUnion<AbstractObjectType>(
        devices,
        m_state,
        additionalEntities,
        [this](const QnVirtualCameraResourcePtr& device) { return device->supportedObjectTypes(); },
        [](const AnalyticsEngineResourcePtr& engine) { return engine->objectTypeIds(); },
        [this](const QString& objectTypeId) { return m_state->objectTypeById(objectTypeId); });
}

std::vector<EngineScope<AbstractObjectType>> StateHelper::compatibleObjectTypeTreeIntersection(
    const QnVirtualCameraResourceList& devices,
    const std::vector<ScopedEntity<AbstractObjectType>>& additionalEntities) const
{
    return compatibleEntityTypeTreeIntersection<AbstractObjectType>(
        devices,
        m_state,
        additionalEntities,
        [](const QnVirtualCameraResourcePtr& device) { return device->supportedObjectTypes(); },
        [](const AnalyticsEngineResourcePtr& engine) { return engine->objectTypeIds(); },
        [this](const QString& objectTypeId) { return m_state->objectTypeById(objectTypeId); });
}

} // namespace nx::analytics::taxonomy
