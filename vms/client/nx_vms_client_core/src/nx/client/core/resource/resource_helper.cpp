#include "resource_helper.h"

#include <common/common_module.h>
#include <core/resource/resource.h>
#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/client/core/watchers/server_time_watcher.h>

namespace nx::vms::client::core {

ResourceHelper::ResourceHelper(QObject* parent):
    base_type(parent),
    QnConnectionContextAware()
{
    const auto timeWatcher = commonModule()->instance<ServerTimeWatcher>();
    connect(timeWatcher, &ServerTimeWatcher::displayOffsetsChanged,
        this, &ResourceHelper::serverTimeOffsetChanged);
}

QString ResourceHelper::resourceId() const
{
    return m_resource ? m_resource->getId().toString() : QString();
}

void ResourceHelper::setResourceId(const QString& id)
{
    if (resourceId() == id)
        return;

    const auto uuid = QnUuid::fromStringSafe(id);
    const auto resource = resourcePool()->getResourceById<QnResource>(uuid);

    if (m_resource)
        m_resource->disconnect(this);

    m_resource = resource;

    if (m_resource)
    {
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

        connect(camera, &QnSecurityCamResource::parameterValueChanged, this,
            [this](const QnResourcePtr& /*resource*/, const QString& param)
            {
                if (param == ResourcePropertyKey::kMediaCapabilities)
                    emit audioSupportedChanged();
                else if (param == ResourcePropertyKey::kIoConfigCapability)
                    emit isIoModuleChanged();
            });
    }

    emit resourceIdChanged();
    emit resourceNameChanged();
    emit resourceStatusChanged();
    emit oldCameraFirmwareChanged();
    emit defaultCameraPasswordChanged();
    emit serverTimeOffsetChanged();
    emit audioSupportedChanged();
    emit isIoModuleChanged();
    emit hasVideoChanged();
}

Qn::ResourceStatus ResourceHelper::resourceStatus() const
{
    return m_resource ? m_resource->getStatus() : Qn::NotDefined;
}

QString ResourceHelper::resourceName() const
{
    return m_resource ? m_resource->getName() : QString();
}

QnResourcePtr ResourceHelper::resource() const
{
    return m_resource;
}

bool ResourceHelper::hasCameraCapability(Qn::CameraCapability capability) const
{
    if (const auto camera = m_resource.dynamicCast<QnSecurityCamResource>())
        return camera->hasCameraCapabilities(capability);

    return false;
}

bool ResourceHelper::hasDefaultCameraPassword() const
{
    return hasCameraCapability(Qn::IsDefaultPasswordCapability);
}

bool ResourceHelper::hasOldCameraFirmware() const
{
    return hasCameraCapability(Qn::IsOldFirmwareCapability);
}

bool ResourceHelper::audioSupported() const
{
    if (const auto camera = m_resource.dynamicCast<QnSecurityCamResource>())
        return camera->isAudioSupported();

    return false;
}

bool ResourceHelper::isIoModule() const
{
    if (const auto camera = m_resource.dynamicCast<QnSecurityCamResource>())
        return camera->isIOModule();

    return false;
}

bool ResourceHelper::hasVideo() const
{
    if (const auto camera = m_resource.dynamicCast<QnSecurityCamResource>())
        return camera->hasVideo();

    return false;
}

qint64 ResourceHelper::serverTimeOffset() const
{
    const auto mediaResource = m_resource.dynamicCast<QnMediaResource>();
    if (!mediaResource)
        return 0;

    const auto timeWatcher = commonModule()->instance<nx::vms::client::core::ServerTimeWatcher>();
    return timeWatcher->displayOffset(mediaResource);
}

} // namespace nx::vms::client::core
