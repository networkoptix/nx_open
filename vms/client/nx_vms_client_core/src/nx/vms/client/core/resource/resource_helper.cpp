// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "resource_helper.h"

#include <QtQml/QQmlEngine>

#include <common/common_module.h>
#include <core/resource/resource.h>
#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/client/core/watchers/server_time_watcher.h>
#include <core/resource/media_server_resource.h>
#include <nx/vms/time/formatter.h>

namespace nx::vms::client::core {

ResourceHelper::ResourceHelper(QObject* parent):
    base_type(parent)
{
    const auto timeWatcher = commonModule()->instance<ServerTimeWatcher>();
    connect(timeWatcher, &ServerTimeWatcher::displayOffsetsChanged,
        this, &ResourceHelper::displayOffsetChanged);

    connect(resourcePool(), &QnResourcePool::resourceRemoved, this,
        [this](const QnResourcePtr& resource)
        {
            if (resourceId() == resource->getId())
                emit resourceRemoved();
        });
}

QnUuid ResourceHelper::resourceId() const
{
    return m_resource ? m_resource->getId() : QnUuid();
}

void ResourceHelper::setResourceId(const QnUuid& id)
{
    if (resourceId() != id)
        setResource(resourcePool()->getResourceById<QnResource>(id));
}

void ResourceHelper::setResource(const QnResourcePtr& value)
{
    if (m_resource)
        m_resource->disconnect(this);

    m_resource = value;

    if (m_resource)
    {
        QQmlEngine::setObjectOwnership(m_resource.get(), QQmlEngine::CppOwnership);

        connect(m_resource, &QnResource::nameChanged,
            this, &ResourceHelper::resourceNameChanged);
        connect(m_resource, &QnResource::statusChanged,
            this, &ResourceHelper::resourceStatusChanged);
    }

    if (const auto camera = m_resource.dynamicCast<QnSecurityCamResource>())
    {
        connect(camera, &QnSecurityCamResource::capabilitiesChanged, this,
            [this]()
            {
                emit defaultCameraPasswordChanged();
                emit oldCameraFirmwareChanged();
            });

        connect(camera, &QnSecurityCamResource::propertyChanged, this,
            [this](
                const QnResourcePtr& resource,
                const QString& key,
                const QString& /*prevValue*/,
                const QString& /*newValue*/)
            {
                if (key == ResourcePropertyKey::kMediaCapabilities)
                    emit audioSupportedChanged();
                else if (key == ResourcePropertyKey::kIoConfigCapability)
                    emit isIoModuleChanged();
            });
    }

    emit resourceChanged();
    emit resourceIdChanged();
    emit resourceNameChanged();
    emit resourceStatusChanged();
    emit oldCameraFirmwareChanged();
    emit defaultCameraPasswordChanged();
    emit displayOffsetChanged();
    emit audioSupportedChanged();
    emit isIoModuleChanged();
    emit isVideoCameraChanged();
    emit hasVideoChanged();
}

QnResource* ResourceHelper::rawResource() const
{
    return m_resource.get();
}

void ResourceHelper::setRawResource(QnResource* value)
{
    setResource(value ? value->toSharedPointer() : QnResourcePtr());
}

nx::vms::api::ResourceStatus ResourceHelper::resourceStatus() const
{
    return m_resource ? m_resource->getStatus() : nx::vms::api::ResourceStatus::undefined;
}

QString ResourceHelper::resourceName() const
{
    return m_resource ? m_resource->getName() : QString();
}

QnResourcePtr ResourceHelper::resource() const
{
    return m_resource;
}

static bool hasCameraCapability(
    const QnResourcePtr& resource, nx::vms::api::DeviceCapability capability)
{
    const auto camera = resource.dynamicCast<QnSecurityCamResource>();
    return camera && camera->hasCameraCapabilities(capability);
}

bool ResourceHelper::hasDefaultCameraPassword() const
{
    return hasCameraCapability(m_resource, nx::vms::api::DeviceCapability::isDefaultPassword);
}

bool ResourceHelper::hasOldCameraFirmware() const
{
    return hasCameraCapability(m_resource, nx::vms::api::DeviceCapability::isOldFirmware);
}

bool ResourceHelper::audioSupported() const
{
    const auto camera = m_resource.dynamicCast<QnSecurityCamResource>();
    return camera && camera->isAudioSupported();
}

bool ResourceHelper::isIoModule() const
{
    const auto camera = m_resource.dynamicCast<QnSecurityCamResource>();
    return camera && camera->isIOModule();
}

bool ResourceHelper::isVideoCamera() const
{
    const auto camera = m_resource.dynamicCast<QnSecurityCamResource>();
    return camera && camera->hasVideo();
}

bool ResourceHelper::hasVideo() const
{
    const auto mediaResource = m_resource.dynamicCast<QnMediaResource>();
    return mediaResource && mediaResource->hasVideo();
}

qint64 ResourceHelper::displayOffset() const
{
    using Watcher = nx::vms::client::core::ServerTimeWatcher;
    const auto timeWatcher = commonModule()->instance<Watcher>();
    const auto mediaResource = m_resource.dynamicCast<QnMediaResource>();

    return !mediaResource || timeWatcher->timeMode() == Watcher::clientTimeMode
        ? nx::vms::time::systemDisplayOffset()
        : timeWatcher->utcOffset(mediaResource);
}

} // namespace nx::vms::client::core
