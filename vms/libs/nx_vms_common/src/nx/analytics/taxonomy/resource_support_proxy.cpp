// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "resource_support_proxy.h"

#include <optional>

#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resource_properties.h>
#include <nx/analytics/taxonomy/utils.h>
#include <nx/fusion/model_functions.h>
#include <nx/reflect/json/deserializer.h>
#include <nx/utils/safe_direct_connection.h>
#include <nx/utils/serialization/qjson.h>
#include <nx/utils/thread/mutex.h>
#include <nx/vms/api/analytics/device_agent_manifest.h>
#include <nx/vms/common/system_context.h>
#include <nx/vms/common/system_context_aware.h>

using nx::vms::api::analytics::DeviceAgentManifest;
using nx::vms::common::SystemContext;
using nx::vms::common::SystemContextAware;

namespace nx::analytics::taxonomy {

struct SupportInfo
{
    std::map<QString /*eventTypeId*/ , std::set<QString /*attributeName*/>> eventTypeSupport;
    std::map<QString /*objectTypeId*/, std::set<QString /*attributeName*/>> objectTypeSupport;
};

SupportInfo supportInfoFromDeviceAgentManifest(const DeviceAgentManifest& deviceAgentManifest)
{
    SupportInfo result;

    for (const QString& eventTypeId: deviceAgentManifest.supportedEventTypeIds)
        result.eventTypeSupport[eventTypeId] = std::set<QString>();

    for (const QString& objectTypeId: deviceAgentManifest.supportedObjectTypeIds)
        result.objectTypeSupport[objectTypeId] = std::set<QString>();

    for (const auto& eventType: deviceAgentManifest.eventTypes)
        result.eventTypeSupport[eventType.id] = std::set<QString>();

    for (const auto& objectType: deviceAgentManifest.objectTypes)
        result.objectTypeSupport[objectType.id] = std::set<QString>();

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

    return result;
}

struct ResourceSupportProxy::Private:
    public QObject,
    public Qn::EnableSafeDirectConnection,
    public SystemContextAware
{
    ResourceSupportProxy* q = nullptr;
    mutable std::map<QnUuid, std::map<QnUuid, SupportInfo>> supportInfoCache;
    mutable nx::Mutex mutex;

    Private(ResourceSupportProxy* q, SystemContext* systemContext):
        SystemContextAware(systemContext),
        q(q)
    {
        directConnect(resourcePropertyDictionary(),
            &QnResourcePropertyDictionary::propertyChanged,
            this,
            &ResourceSupportProxy::Private::manifestsMaybeUpdated);
        directConnect(resourcePropertyDictionary(),
            &QnResourcePropertyDictionary::propertyRemoved,
            this,
            &ResourceSupportProxy::Private::manifestsMaybeUpdated);
    }

    ~Private()
    {
        directDisconnectAll();
    }

    void manifestsMaybeUpdated(const QnUuid& resourceId, const QString& key)
    {
        {
            NX_MUTEX_LOCKER lock(&mutex);
            if (key != QnVirtualCameraResource::kDeviceAgentManifestsProperty)
                return;

            supportInfoCache.erase(resourceId);
        }
        emit q->manifestsMaybeUpdated();
    }

    void fillCacheIfNeeded(QnUuid deviceId) const
    {
        if (supportInfoCache.contains(deviceId))
            return;

        const QnVirtualCameraResourcePtr device =
            systemContext()->resourcePool()->getResourceById<QnVirtualCameraResource>(deviceId);

        if (!device)
            return;

        for (const auto& [engineId, manifest]: device->deviceAgentManifests())
        {
            SupportInfo supportInfo = supportInfoFromDeviceAgentManifest(manifest);
            supportInfoCache[deviceId][engineId] = std::move(supportInfo);
        }
    }

    bool isEventTypeSupported(const QString& eventTypeId, QnUuid deviceId, QnUuid engineId) const
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

    bool isObjectTypeSupported(const QString& objectTypeId, QnUuid deviceId, QnUuid engineId) const
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
        QnUuid deviceId,
        QnUuid engineId) const
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
        QnUuid deviceId,
        QnUuid engineId) const
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
    EntityType entityType, const QString& entityTypeId, QnUuid deviceId, QnUuid engineId) const
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
    QnUuid deviceId,
    QnUuid engineId) const
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
