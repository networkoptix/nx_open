// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "resource_support_proxy.h"

#include <optional>

#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resource_properties.h>
#include <nx/analytics/taxonomy/utils.h>
#include <nx/fusion/model_functions.h>
#include <nx/reflect/json/deserializer.h>
#include <nx/utils/json/qjson.h>
#include <nx/utils/safe_direct_connection.h>
#include <nx/utils/thread/mutex.h>
#include <nx/vms/api/analytics/device_agent_manifest.h>
#include <nx/vms/common/resource/analytics_engine_resource.h>
#include <nx/vms/common/system_context.h>
#include <nx/vms/common/system_context_aware.h>

using nx::vms::api::analytics::DeviceAgentManifest;
using nx::vms::api::analytics::EngineManifest;
using nx::vms::common::SystemContext;
using nx::vms::common::SystemContextAware;

namespace api = nx::vms::api;

namespace nx::analytics::taxonomy {

struct SupportInfo
{
    std::map<QString /*eventTypeId*/ , std::set<QString /*attributeName*/>> eventTypeSupport;
    std::map<QString /*objectTypeId*/, std::set<QString /*attributeName*/>> objectTypeSupport;
};

SupportInfo extractSupportInfo(
    const DeviceAgentManifest& deviceAgentManifest,
    const EngineManifest& engineManifest)
{
    SupportInfo result;

    for (const QString& eventTypeId: deviceAgentManifest.supportedEventTypeIds)
        result.eventTypeSupport[eventTypeId] = std::set<QString>();

    for (const QString& objectTypeId: deviceAgentManifest.supportedObjectTypeIds)
        result.objectTypeSupport[objectTypeId] = std::set<QString>();

    for (const auto& supportedType: deviceAgentManifest.supportedTypes)
    {
        if (supportedType.objectTypeId.isEmpty() && !supportedType.eventTypeId.isEmpty())
        {
            result.eventTypeSupport[supportedType.eventTypeId] =
                std::set<QString>(
                    supportedType.attributes.begin(),
                    supportedType.attributes.end());
        }
        else if (!supportedType.objectTypeId.isEmpty() && supportedType.eventTypeId.isEmpty())
        {
            result.objectTypeSupport[supportedType.objectTypeId] =
                std::set<QString>(
                    supportedType.attributes.begin(),
                    supportedType.attributes.end());
        }
    }

    auto addBaseTypesForDeprecated =
        [](auto* target, const auto& types)
        {
            for (const auto& type: types)
            {
                target->emplace(type.id, std::set<QString>());
                if (auto base = type.base.value_or(QString()); !base.isEmpty())
                    target->emplace(std::move(base), std::set<QString>());
            }
        };
    addBaseTypesForDeprecated(&result.eventTypeSupport, deviceAgentManifest.eventTypes);
    addBaseTypesForDeprecated(&result.objectTypeSupport, deviceAgentManifest.objectTypes);

    auto addBaseTypes =
        [](auto* target, const auto& types)
        {
            for (const auto& type: types)
            {
                if (auto base = type.base.value_or(QString()); !base.isEmpty())
                {
                    if (target->contains(type.id))
                        target->emplace(std::move(base), std::set<QString>());
                }
            }
        };
    addBaseTypes(&result.eventTypeSupport, deviceAgentManifest.typeLibrary.eventTypes);
    addBaseTypes(&result.objectTypeSupport, deviceAgentManifest.typeLibrary.objectTypes);
    addBaseTypes(&result.eventTypeSupport, engineManifest.typeLibrary.eventTypes);
    addBaseTypes(&result.objectTypeSupport, engineManifest.typeLibrary.objectTypes);

    return result;
}

struct ResourceSupportProxy::Private:
    public QObject,
    public Qn::EnableSafeDirectConnection,
    public SystemContextAware
{
    ResourceSupportProxy* q = nullptr;
    mutable std::map<nx::Uuid, std::map<nx::Uuid, SupportInfo>> supportInfoCache;
    mutable nx::Mutex mutex;

    Private(ResourceSupportProxy* q, SystemContext* systemContext):
        SystemContextAware(systemContext),
        q(q)
    {
        directConnect(resourcePropertyDictionary(),
            &QnResourcePropertyDictionary::propertyChanged,
            [this](auto... args) { manifestsMaybeUpdated(args...); });
        directConnect(resourcePropertyDictionary(),
            &QnResourcePropertyDictionary::propertyRemoved,
            [this](auto... args) { manifestsMaybeUpdated(args...); });
    }

    ~Private()
    {
        directDisconnectAll();
    }

    void manifestsMaybeUpdated(const nx::Uuid& resourceId, const QString& key)
    {
        static const QStringList kRelatedKeys = {
            api::device_properties::kDeviceAgentManifestsKey,
            api::device_properties::kCompatibleAnalyticsEnginesProperty,
            api::device_properties::kUserEnabledAnalyticsEnginesProperty,
        };

        if (!kRelatedKeys.contains(key))
            return;

        {
            NX_MUTEX_LOCKER lock(&mutex);
            supportInfoCache.erase(resourceId);
        }
        emit q->manifestsMaybeUpdated();
    }

    void fillCacheIfNeeded(nx::Uuid deviceId) const
    {
        if (supportInfoCache.contains(deviceId))
            return;

        const auto* resourcePool = systemContext()->resourcePool();
        const QnVirtualCameraResourcePtr device =
            resourcePool->getResourceById<QnVirtualCameraResource>(deviceId);

        if (!device)
            return;

        auto& deviceCache = supportInfoCache[deviceId];
        for (const auto& [engineId, deviceAgentManifest]: device->deviceAgentManifests())
        {
            EngineManifest engineManifest;
            const auto engine =
                resourcePool->getResourceById<nx::vms::common::AnalyticsEngineResource>(engineId);
            if (engine)
                engineManifest = engine->manifest();

            deviceCache[engineId] = extractSupportInfo(deviceAgentManifest, engineManifest);
        }
    }

    bool isEventTypeSupported(const QString& eventTypeId, nx::Uuid deviceId, nx::Uuid engineId) const
    {
        if (!NX_ASSERT(!deviceId.isNull()))
            return false;

        if (!NX_ASSERT(!engineId.isNull()))
            return false;

        if (!supportInfoCache.contains(deviceId))
            return false;

        if (!supportInfoCache[deviceId].contains(engineId))
            return false;

        return supportInfoCache[deviceId][engineId].eventTypeSupport.contains(eventTypeId);
    }

    bool isObjectTypeSupported(const QString& objectTypeId, nx::Uuid deviceId, nx::Uuid engineId) const
    {
        if (!NX_ASSERT(!deviceId.isNull()))
            return false;

        if (!NX_ASSERT(!engineId.isNull()))
            return false;

        if (!supportInfoCache.contains(deviceId))
            return false;

        if (!supportInfoCache[deviceId].contains(engineId))
            return false;

        const auto& objectTypeSupport = supportInfoCache[deviceId][engineId].objectTypeSupport;

        // TODO: Remove later. For detailed information see the comment above
        // maybeUnscopedExtendedObjectTypeId().
        if (objectTypeSupport.contains(objectTypeId))
            return true;
        const QString extendedObjectTypeId = maybeUnscopedExtendedObjectTypeId(objectTypeId);
        return objectTypeSupport.contains(extendedObjectTypeId);
    }

    bool isEventTypeAttributeSupported(
        const QString& eventTypeId,
        const QString& fullAttributeName,
        nx::Uuid deviceId,
        nx::Uuid engineId) const
    {
        if (!isEventTypeSupported(eventTypeId, deviceId, engineId))
            return false;

        const std::set<QString>& supportedAttributes =
            supportInfoCache[deviceId][engineId].eventTypeSupport[eventTypeId];

        const auto it = std::lower_bound(
            supportedAttributes.begin(),
            supportedAttributes.end(),
            fullAttributeName);

        if (it == supportedAttributes.cend())
            return false;

        return *it == fullAttributeName || it->startsWith(fullAttributeName + ".");
    }

    bool isObjectTypeAttributeSupported(
        const QString& objectTypeId,
        const QString& fullAttributeName,
        nx::Uuid deviceId,
        nx::Uuid engineId) const
    {
        if (!isObjectTypeSupported(objectTypeId, deviceId, engineId))
            return false;

        const std::set<QString>& supportedAttributes =
            supportInfoCache[deviceId][engineId].objectTypeSupport[objectTypeId];

        const auto it = std::lower_bound(
            supportedAttributes.begin(),
            supportedAttributes.end(),
            fullAttributeName);

        if (it == supportedAttributes.cend())
            return false;

        return *it == fullAttributeName || it->startsWith(fullAttributeName + ".");
    }
};

ResourceSupportProxy::ResourceSupportProxy(SystemContext* systemContext):
    d(new Private(this, systemContext))
{
}

ResourceSupportProxy::~ResourceSupportProxy()
{
}

bool ResourceSupportProxy::isEntityTypeSupported(
    EntityType entityType, const QString& entityTypeId, nx::Uuid deviceId, nx::Uuid engineId) const
{
    NX_MUTEX_LOCKER lock(&d->mutex);

    if (!NX_ASSERT(!deviceId.isNull()))
        return false;

    d->fillCacheIfNeeded(deviceId);
    if (!d->supportInfoCache.contains(deviceId))
        return false;

    if (entityType == EntityType::eventType)
    {
        if (!engineId.isNull())
            return d->isEventTypeSupported(entityTypeId, deviceId, engineId);

        for (const auto& [supportedEngineId, _]: d->supportInfoCache[deviceId])
        {
            if (d->isEventTypeSupported(entityTypeId, deviceId, supportedEngineId))
                return true;
        }
    }
    else if (entityType == EntityType::objectType)
    {
        if (!engineId.isNull())
        return d->isObjectTypeSupported(entityTypeId, deviceId, engineId);

        for (const auto& [supportedEngineId, _]: d->supportInfoCache[deviceId])
        {
            if (d->isObjectTypeSupported(entityTypeId, deviceId, supportedEngineId))
                return true;
        }
    }

    return false;
}

bool ResourceSupportProxy::isEntityTypeAttributeSupported(
    EntityType entityType,
    const QString& entityTypeId,
    const QString& fullAttributeName,
    nx::Uuid deviceId,
    nx::Uuid engineId) const
{
    NX_MUTEX_LOCKER lock(&d->mutex);

    if (!NX_ASSERT(!deviceId.isNull()))
        return false;

    d->fillCacheIfNeeded(deviceId);
    if (!d->supportInfoCache.contains(deviceId))
        return false;

    if (!engineId.isNull())
    {
        if (entityType == EntityType::eventType)
        {
            return d->isEventTypeAttributeSupported(
                entityTypeId, fullAttributeName, deviceId, engineId);
        }
        else if (entityType == EntityType::objectType)
        {
            return d->isObjectTypeAttributeSupported(
                entityTypeId, fullAttributeName, deviceId, engineId);
        }
    }

    for (const auto& [supportedEngineId, _]: d->supportInfoCache[deviceId])
    {
        if ((entityType == EntityType::eventType && d->isEventTypeAttributeSupported(
            entityTypeId, fullAttributeName, deviceId, supportedEngineId))
            || (entityType == EntityType::objectType && d->isObjectTypeAttributeSupported(
            entityTypeId, fullAttributeName, deviceId, supportedEngineId)))
        {
            return true;
        }
    }

    return false;
}

} // namespace nx::analytics::taxonomy
