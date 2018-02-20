#include "two_way_audio_availability_watcher.h"

#include <common/common_module.h>
#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>
#include <client_core/client_core_module.h>
#include <utils/license_usage_helper.h>

namespace nx {
namespace client {
namespace core {

TwoWayAudioAvailabilityWatcher::TwoWayAudioAvailabilityWatcher(QObject* parent):
    base_type(parent)
{
}

TwoWayAudioAvailabilityWatcher::~TwoWayAudioAvailabilityWatcher()
{
}

void TwoWayAudioAvailabilityWatcher::setResourceId(const QnUuid& uuid)
{
    if (m_camera && m_camera->getId() == uuid)
        return;

    if (m_camera)
        m_camera->disconnect(this);

    const auto pool = qnClientCoreModule->commonModule()->resourcePool();
    const auto camera = pool->getResourceById<QnVirtualCameraResource>(uuid);
    m_camera = camera->hasTwoWayAudio() ? camera : QnVirtualCameraResourcePtr();
    m_licenseHelper.reset(m_camera && m_camera->isIOModule()
        ? new QnSingleCamLicenseStatusHelper(m_camera)
        : nullptr);

    if (m_camera)
    {
        connect(m_camera, &QnVirtualCameraResource::statusChanged,
            this, &TwoWayAudioAvailabilityWatcher::updateAvailability);

        if (m_licenseHelper)
        {
            connect(m_licenseHelper, &QnSingleCamLicenseStatusHelper::licenseStatusChanged,
                this, &TwoWayAudioAvailabilityWatcher::updateAvailability);
        }
    }

    updateAvailability();
}

bool TwoWayAudioAvailabilityWatcher::available() const
{
    return m_available;
}

void TwoWayAudioAvailabilityWatcher::updateAvailability()
{
    const bool isAvailable =
        [this]()
        {
            if (!m_camera)
                return false;

            if (m_camera->getStatus() != Qn::Online && m_camera->getStatus() != Qn::Recording)
                return false;

            if (m_licenseHelper)
                return m_licenseHelper->status() == QnLicenseUsageStatus::used;

            return true;
        }();

    qDebug() << "--------- update availability" << isAvailable;
    setAvailable(isAvailable);
}

void TwoWayAudioAvailabilityWatcher::setAvailable(bool value)
{
    if (m_available == value)
        return;

    m_available = value;
    emit availabilityChanged();
}


} // namespace core
} // namespace client
} // namespace nx

