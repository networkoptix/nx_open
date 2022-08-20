// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "resource_helper.h"

#include <QtQml/QQmlEngine>

#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/client/core/system_context.h>
#include <nx/vms/client/core/watchers/server_time_watcher.h>
#include <nx/vms/time/formatter.h>

namespace nx::vms::client::core {

ResourceHelper::ResourceHelper(SystemContext* systemContext, QObject* parent):
    QObject(parent),
    SystemContextAware(systemContext)
{
    initialize();
}

ResourceHelper::ResourceHelper():
    QObject(),
    SystemContextAware(SystemContext::fromQmlContext(this))
{
}

void ResourceHelper::initialize()
{
    if (m_initialized)
        return;

    m_initialized = true;

    const auto timeWatcher = systemContext()->serverTimeWatcher();
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
    initialize();
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
        initialize();
        QQmlEngine::setObjectOwnership(m_resource.get(), QQmlEngine::CppOwnership);

        connect(m_resource.get(), &QnResource::nameChanged,
            this, &ResourceHelper::resourceNameChanged);
        connect(m_resource.get(), &QnResource::statusChanged,
            this, &ResourceHelper::resourceStatusChanged);
    }

    if (const auto camera = m_resource.dynamicCast<QnSecurityCamResource>())
    {
        connect(camera.get(), &QnSecurityCamResource::capabilitiesChanged, this,
            [this]()
            {
                emit defaultCameraPasswordChanged();
                emit oldCameraFirmwareChanged();
            });

        connect(camera.get(), &QnSecurityCamResource::propertyChanged, this,
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

int ResourceHelper::resourceStatus() const
{
    return static_cast<int>(m_resource
        ? m_resource->getStatus()
        : nx::vms::api::ResourceStatus::undefined);
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
    const auto mediaResource = m_resource.dynamicCast<QnMediaResource>();
    return mediaResource
        ? serverTimeWatcher()->displayOffset(m_resource.dynamicCast<QnMediaResource>())
        : 0;
}

} // namespace nx::vms::client::core
