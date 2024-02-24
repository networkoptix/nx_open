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
        QQmlEngine::setObjectOwnership(m_resource.get(), QQmlEngine::CppOwnership);

        m_connections << connect(m_resource.get(), &QnResource::nameChanged,
            this, &ResourceHelper::resourceNameChanged);
        m_connections << connect(m_resource.get(), &QnResource::statusChanged,
            this, &ResourceHelper::resourceStatusChanged);

        if (auto systemContext = SystemContext::fromResource(m_resource); NX_ASSERT(systemContext))
        {
            const auto timeWatcher = systemContext->serverTimeWatcher();
            m_connections << connect(timeWatcher, &ServerTimeWatcher::timeZoneChanged,
                this, &ResourceHelper::displayOffsetChanged);

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

    if (const auto camera = m_resource.dynamicCast<QnSecurityCamResource>())
    {
        m_connections << connect(camera.get(), &QnSecurityCamResource::capabilitiesChanged, this,
            [this]()
            {
                emit defaultCameraPasswordChanged();
                emit oldCameraFirmwareChanged();
            });

        m_connections << connect(camera.get(), &QnSecurityCamResource::propertyChanged, this,
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
    return camera && (camera->isAudioSupported() || !camera->audioInputDeviceId().isNull());
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

// FIXME: #sivanov Used in the only place in the mobile client.
qint64 ResourceHelper::displayOffset() const
{
    using namespace std::chrono;

    if (!m_resource)
        return 0;

    auto systemContext = SystemContext::fromResource(m_resource);
    if (!NX_ASSERT(systemContext))
        return 0;

    if (systemContext->serverTimeWatcher()->timeMode() == ServerTimeWatcher::clientTimeMode)
        return 0;

    const milliseconds resourceOffset = seconds(ServerTimeWatcher::timeZone(
        m_resource.dynamicCast<QnMediaResource>()).offsetFromUtc(QDateTime::currentDateTime()));
    const milliseconds localOffset = seconds(QDateTime::currentDateTime().offsetFromUtc());

    return (resourceOffset - localOffset).count();
}

} // namespace nx::vms::client::core
