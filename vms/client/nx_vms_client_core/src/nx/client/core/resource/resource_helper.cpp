#include "resource_helper.h"

#include <common/common_module.h>
#include <core/resource/resource.h>
#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/client/core/watchers/server_time_watcher.h>
#include <core/resource/media_server_resource.h>
#include <nx/vms/time/formatter.h>

namespace nx::vms::client::core {

ResourceHelper::ResourceHelper(QObject* parent):
    base_type(parent),
    QnConnectionContextAware()
{
    const auto timeWatcher = commonModule()->instance<ServerTimeWatcher>();
    connect(timeWatcher, &ServerTimeWatcher::displayOffsetsChanged,
        this, &ResourceHelper::displayOffsetChanged);
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
    emit displayOffsetChanged();
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
    const auto camera = m_resource.dynamicCast<QnSecurityCamResource>();
    return camera && camera->hasCameraCapabilities(capability);
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
    const auto camera = m_resource.dynamicCast<QnSecurityCamResource>();
    return camera && camera->isAudioSupported();
}

bool ResourceHelper::isIoModule() const
{
    const auto camera = m_resource.dynamicCast<QnSecurityCamResource>();
    return camera && camera->isIOModule();
}

bool ResourceHelper::hasVideo() const
{
    const auto camera = m_resource.dynamicCast<QnSecurityCamResource>();
    return camera && camera->hasVideo();
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
