// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "state.h"

#include <type_traits>

#include <nx/analytics/taxonomy/integration.h>
#include <nx/analytics/taxonomy/engine.h>
#include <nx/analytics/taxonomy/group.h>
#include <nx/analytics/taxonomy/enum_type.h>
#include <nx/analytics/taxonomy/color_type.h>
#include <nx/analytics/taxonomy/event_type.h>
#include <nx/analytics/taxonomy/object_type.h>

using namespace nx::vms::api::analytics;

namespace nx::analytics::taxonomy {

template<typename ResultMap, typename EntityMap>
ResultMap serializeEntityMap(const EntityMap& entityMap)
{
    ResultMap result;

    for (const auto& [id, entity]: entityMap)
    {
        using KeyType =
            typename std::remove_cv_t<std::remove_reference_t<typename ResultMap::key_type>>;

        if constexpr (std::is_same_v<KeyType, QString>)
            result[id] = entity->serialize();
        else
            result[nx::Uuid::fromStringSafe(id)] = entity->serialize();
    }

    return result;
}

template<typename Map>
void assignParent(Map* inOutMap, QObject* parent)
{
    for (auto& [_, item]: *inOutMap)
        item->setParent(parent);
}

State::State(
    InternalState state,
    std::unique_ptr<AbstractResourceSupportProxy> resourceSupportProxy)
    :
    m_internalState(std::move(state)),
    m_resourceSupportProxy(std::move(resourceSupportProxy))
{
    assignParent(&m_internalState.integrationById, this);
    assignParent(&m_internalState.engineById, this);
    assignParent(&m_internalState.groupById, this);
    assignParent(&m_internalState.eventTypeById, this);
    assignParent(&m_internalState.objectTypeById, this);
    assignParent(&m_internalState.enumTypeById, this);
    assignParent(&m_internalState.colorTypeById, this);
}

void State::refillCache() const
{
    m_cachedIntegrations.clear();
    m_cachedEngines.clear();
    m_cachedGroups.clear();
    m_cachedObjectType.clear();
    m_cachedRootObjectType.clear();
    m_cachedEventType.clear();
    m_cachedRootEventType.clear();
    m_cachedEnumTypes.clear();
    m_cachedColorTypes.clear();

    for (const auto& [integrationId, integration]: m_internalState.integrationById)
        m_cachedIntegrations.push_back(integration);

    for (const auto& [engineId, engine]: m_internalState.engineById)
        m_cachedEngines.push_back(engine);

    for (const auto& [groupId, group]: m_internalState.groupById)
        m_cachedGroups.push_back(group);

    for (const auto& [objectTypeId, objectType]: m_internalState.objectTypeById)
    {
        m_cachedObjectType.push_back(objectType);
        if (!objectType->base())
            m_cachedRootObjectType.push_back(objectType);
    }

    for (const auto& [eventTypeId, eventType]: m_internalState.eventTypeById)
    {
        m_cachedEventType.push_back(eventType);
        if (!eventType->base())
            m_cachedRootEventType.push_back(eventType);
    }

    for (const auto& [enumTypeId, enumType]: m_internalState.enumTypeById)
        m_cachedEnumTypes.push_back(enumType);

    for (const auto& [colorTypeId, colorType]: m_internalState.colorTypeById)
        m_cachedColorTypes.push_back(colorType);
}

std::vector<AbstractIntegration*> State::integrations() const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    if (m_cachedIntegrations.empty())
        refillCache();

    return m_cachedIntegrations;
}

std::vector<AbstractEngine*> State::engines() const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    if (m_cachedEngines.empty())
        refillCache();

    return m_cachedEngines;
}

std::vector<AbstractGroup*> State::groups() const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    if (m_cachedGroups.empty())
        refillCache();

    return m_cachedGroups;
}

std::vector<AbstractObjectType*> State::objectTypes() const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    if (m_cachedObjectType.empty())
        refillCache();

    return m_cachedObjectType;
}

std::vector<AbstractEventType*> State::eventTypes() const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    if (m_cachedEventType.empty())
        refillCache();

    return m_cachedEventType;
}

std::vector<AbstractEnumType*> State::enumTypes() const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    if (m_cachedEnumTypes.empty())
        refillCache();

    return m_cachedEnumTypes;
}

std::vector<AbstractColorType*> State::colorTypes() const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    if (m_cachedColorTypes.empty())
        refillCache();

    return m_cachedColorTypes;
}

std::vector<AbstractObjectType*> State::rootObjectTypes() const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    if (m_cachedRootObjectType.empty())
        refillCache();

    return m_cachedRootObjectType;
}

std::vector<AbstractEventType*> State::rootEventTypes() const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    if (m_cachedRootEventType.empty())
        refillCache();

    return m_cachedRootEventType;
}

AbstractObjectType* State::objectTypeById(const QString& objectTypeId) const
{
    return m_internalState.getTypeById<ObjectType>(objectTypeId);
}

AbstractEventType* State::eventTypeById(const QString& eventTypeId) const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    return m_internalState.getTypeById<EventType>(eventTypeId);
}

AbstractIntegration* State::integrationById(const QString& integrationId) const
{
    return m_internalState.getTypeById<Integration>(integrationId);
}

AbstractEngine* State::engineById(const QString& engineId) const
{
    return m_internalState.getTypeById<Engine>(engineId);
}

AbstractGroup* State::groupById(const QString& groupId) const
{
    return m_internalState.getTypeById<Group>(groupId);
}

AbstractEnumType* State::enumTypeById(const QString& enumTypeId) const
{
    return m_internalState.getTypeById<EnumType>(enumTypeId);
}

AbstractColorType* State::colorTypeById(const QString& colorTypeId) const
{
    return m_internalState.getTypeById<ColorType>(colorTypeId);
}

Descriptors State::serialize() const
{
    Descriptors result;

    result.pluginDescriptors = serializeEntityMap<PluginDescriptorMap>(
        m_internalState.integrationById);
    result.engineDescriptors = serializeEntityMap<EngineDescriptorMap>(
        m_internalState.engineById);
    result.groupDescriptors = serializeEntityMap<GroupDescriptorMap>(
        m_internalState.groupById);
    result.enumTypeDescriptors =
        serializeEntityMap<EnumTypeDescriptorMap>(m_internalState.enumTypeById);
    result.colorTypeDescriptors =
        serializeEntityMap<ColorTypeDescriptorMap>(m_internalState.colorTypeById);
    result.eventTypeDescriptors =
        serializeEntityMap<EventTypeDescriptorMap>(m_internalState.eventTypeById);
    result.objectTypeDescriptors =
        serializeEntityMap<ObjectTypeDescriptorMap>(m_internalState.objectTypeById);

    return result;
}

AbstractResourceSupportProxy* State::resourceSupportProxy() const
{
    return m_resourceSupportProxy.get();
}

} // namespace nx::analytics::taxonomy
