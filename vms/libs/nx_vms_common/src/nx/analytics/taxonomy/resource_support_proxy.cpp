// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "resource_support_proxy.h"

#include <optional>

#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/fusion/model_functions.h>
#include <nx/utils/thread/mutex.h>
#include <nx/vms/api/analytics/device_agent_manifest.h>

using nx::vms::api::analytics::DeviceAgentManifest;

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

struct ResourceSupportProxy::Private
{
    QnResourcePool* resourcePool = nullptr;
    mutable std::map<QnUuid, std::map<QnUuid, SupportInfo>> supportInfoCache;
    mutable nx::Mutex mutex;

    void fillCacheIfNeeded(QnUuid deviceId) const
    {
        if (supportInfoCache.contains(deviceId))
            return;

        const QnVirtualCameraResourcePtr device =
            resourcePool->getResourceById<QnVirtualCameraResource>(deviceId);

        if (!device)
            return;

        const QString serializedManifests =
            device->getProperty(QnVirtualCameraResource::kDeviceAgentManifestsProperty);

        using DeviceAgentManifests = std::map<QnUuid, DeviceAgentManifest>;
        DeviceAgentManifests manifests = QJson::deserialized<DeviceAgentManifests>(
            serializedManifests.toUtf8());

        for (const auto& [engineId, manifest]: manifests)
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

    bool isObjectTypeSupported(const QString& eventTypeId, QnUuid deviceId, QnUuid engineId) const
    {
        if (!NX_ASSERT(!deviceId.isNull()))
            return false;

        if (!NX_ASSERT(!engineId.isNull()))
            return false;

        if (!supportInfoCache.contains(deviceId))
            return false;

        if (!supportInfoCache[deviceId].contains(engineId))
            return false;

        return supportInfoCache[deviceId][engineId].objectTypeSupport.contains(eventTypeId);
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

ResourceSupportProxy::ResourceSupportProxy(QnResourcePool* resourcePool):
    d(new Private{.resourcePool = resourcePool})
{
    NX_ASSERT(d->resourcePool);
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
