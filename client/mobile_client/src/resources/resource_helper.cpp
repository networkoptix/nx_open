#include "resource_helper.h"

#include <common/common_module.h>
#include <core/resource/resource.h>
#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/client/core/watchers/server_time_watcher.h>

QnResourceHelper::QnResourceHelper(QObject* parent):
    base_type(parent),
    QnConnectionContextAware()
{
    const auto timeWatcher = commonModule()->instance<nx::client::core::ServerTimeWatcher>();
    connect(timeWatcher, &nx::client::core::ServerTimeWatcher::displayOffsetsChanged,
        this, &QnResourceHelper::serverTimeOffsetChanged);
}

QString QnResourceHelper::resourceId() const
{
    return m_resource ? m_resource->getId().toString() : QString();
}

void QnResourceHelper::setResourceId(const QString& id)
{
    if (resourceId() == id)
        return;

    const auto uuid = QnUuid::fromStringSafe(id);
    const auto resource = resourcePool()->getResourceById<QnResource>(uuid);

    if (m_resource)
        m_resource->disconnect(this);

    m_resource = resource;

    connect(m_resource, &QnResource::nameChanged,
        this, &QnResourceHelper::resourceNameChanged);
    connect(m_resource, &QnResource::statusChanged,
        this, &QnResourceHelper::resourceStatusChanged);

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
                if (param == nx::media::kCameraMediaCapabilityParamName)
                    emit audioSupportedChanged();
                else if (param == Qn::IO_CONFIG_PARAM_NAME)
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

Qn::ResourceStatus QnResourceHelper::resourceStatus() const
{
    return m_resource ? m_resource->getStatus() : Qn::NotDefined;
}

QString QnResourceHelper::resourceName() const
{
    return m_resource ? m_resource->getName() : QString();
}

QnResourcePtr QnResourceHelper::resource() const
{
    return m_resource;
}

bool QnResourceHelper::hasCameraCapability(Qn::CameraCapability capability) const
{
    if (const auto camera = m_resource.dynamicCast<QnSecurityCamResource>())
        return camera->hasCameraCapabilities(capability);

    return false;
}

bool QnResourceHelper::hasDefaultCameraPassword() const
{
    return hasCameraCapability(Qn::isDefaultPasswordCapability);
}

bool QnResourceHelper::hasOldCameraFirmware() const
{
    return hasCameraCapability(Qn::isOldFirmwareCapability);
}

bool QnResourceHelper::audioSupported() const
{
    if (const auto camera = m_resource.dynamicCast<QnSecurityCamResource>())
        return camera->isAudioSupported();

    return false;
}

bool QnResourceHelper::isIoModule() const
{
    if (const auto camera = m_resource.dynamicCast<QnSecurityCamResource>())
        return camera->isIOModule();

    return false;
}

bool QnResourceHelper::hasVideo() const
{
    if (const auto camera = m_resource.dynamicCast<QnSecurityCamResource>())
        return camera->hasVideo();

    return false;
}

qint64 QnResourceHelper::serverTimeOffset() const
{
    const auto mediaResource = m_resource.dynamicCast<QnMediaResource>();
    if (!mediaResource)
        return 0;

    const auto timeWatcher = commonModule()->instance<nx::client::core::ServerTimeWatcher>();
    return timeWatcher->displayOffset(mediaResource);
}
