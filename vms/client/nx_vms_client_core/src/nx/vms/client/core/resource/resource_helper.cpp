// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "resource_helper.h"

#include <QtCore/QTimeZone>

#include <core/resource/camera_resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/client/core/system_context.h>
#include <nx/vms/client/core/watchers/server_time_watcher.h>
#include <nx/vms/time/formatter.h>

namespace nx::vms::client::core {

ResourceHelper::ResourceHelper(QObject* parent):
    QObject(parent)
{
}

void ResourceHelper::setResource(const QnResourcePtr& value)
{
    m_connections.reset();

    m_resource = value;

    if (m_resource)
    {
        m_connections << connect(m_resource.get(), &QnResource::nameChanged,
            this, &ResourceHelper::resourceNameChanged);
        m_connections << connect(m_resource.get(), &QnResource::statusChanged,
            this, &ResourceHelper::resourceStatusChanged);

        m_connections << connect(m_resource.get(), &QnResource::propertyChanged, this,
            [this](
                const QnResourcePtr& /*resource*/,
                const QString& key,
                const QString& /*prevValue*/,
                const QString& /*newValue*/)
            {
                if (key == nx::vms::api::device_properties::kMediaCapabilities)
                    emit audioSupportedChanged();
                else if (key == nx::vms::api::device_properties::kIoConfigCapability)
                    emit isIoModuleChanged();
                else if (key == nx::vms::api::device_properties::kNoVideoSupport)
                    emit hasVideoChanged();
            });

        if (auto systemContext = SystemContext::fromResource(m_resource); NX_ASSERT(systemContext))
        {
            const auto timeWatcher = systemContext->serverTimeWatcher();
            m_connections << connect(timeWatcher, &ServerTimeWatcher::timeZoneChanged,
                this, &ResourceHelper::timeZoneChanged);

            m_connections << connect(systemContext->resourcePool(),
                &QnResourcePool::resourcesRemoved,
                this,
                [this](const QnResourceList& resources)
                {
                    if (resources.contains(m_resource))
                    {
                        emit resourceRemoved();
                        setResource({});
                    }
                });
        }
    }

    if (const auto camera = m_resource.dynamicCast<QnVirtualCameraResource>())
    {
        m_connections << connect(camera.get(), &QnVirtualCameraResource::capabilitiesChanged, this,
            [this]()
            {
                emit defaultCameraPasswordChanged();
                emit oldCameraFirmwareChanged();
            });
    }

    emit resourceChanged();
    emit resourceNameChanged();
    emit resourceStatusChanged();
    emit oldCameraFirmwareChanged();
    emit defaultCameraPasswordChanged();
    emit timeZoneChanged();
    emit audioSupportedChanged();
    emit isIoModuleChanged();
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
    const auto camera = resource.dynamicCast<QnVirtualCameraResource>();
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
    const auto camera = m_resource.dynamicCast<QnVirtualCameraResource>();
    return camera && (camera->isAudioSupported() || !camera->audioInputDeviceId().isNull());
}

bool ResourceHelper::isIoModule() const
{
    const auto camera = m_resource.dynamicCast<QnVirtualCameraResource>();
    return camera && camera->isIOModule();
}

bool ResourceHelper::hasVideo() const
{
    const auto mediaResource = m_resource.dynamicCast<QnMediaResource>();
    return mediaResource && mediaResource->hasVideo();
}

bool ResourceHelper::isCamera() const
{
    return !m_resource.dynamicCast<QnVirtualCameraResource>().isNull();
}

bool ResourceHelper::isLayout() const
{
    return !m_resource.dynamicCast<QnLayoutResource>().isNull();
}

QTimeZone ResourceHelper::timeZone() const
{
    if (!m_resource)
        return QTimeZone::LocalTime;

    auto systemContext = SystemContext::fromResource(m_resource);
    if (!NX_ASSERT(systemContext))
        return QTimeZone::LocalTime;

    if (systemContext->serverTimeWatcher()->timeMode() == ServerTimeWatcher::clientTimeMode)
        return QTimeZone::LocalTime;

    return ServerTimeWatcher::timeZone(m_resource.dynamicCast<QnMediaResource>());
}

} // namespace nx::vms::client::core
