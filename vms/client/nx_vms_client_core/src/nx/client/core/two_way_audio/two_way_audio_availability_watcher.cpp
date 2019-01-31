#include "two_way_audio_availability_watcher.h"

#include <common/common_module.h>
#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_access/global_permissions_manager.h>
#include <client_core/client_core_module.h>
#include <utils/license_usage_helper.h>
#include <nx/client/core/watchers/user_watcher.h>

namespace nx::vms::client::core {

TwoWayAudioAvailabilityWatcher::TwoWayAudioAvailabilityWatcher(QObject* parent):
    base_type(parent)
{
    const auto manager = globalPermissionsManager();
    const auto userWatcher = commonModule()->instance<UserWatcher>();

    connect(userWatcher, &UserWatcher::userChanged,
        this, &TwoWayAudioAvailabilityWatcher::updateAvailability);
    connect(manager, &QnGlobalPermissionsManager::globalPermissionsChanged, this,
        [this, userWatcher]
            (const QnResourceAccessSubject& subject, GlobalPermissions /*permissions*/)
        {
            const auto user = userWatcher->user();
            if (subject != user)
                return;

            updateAvailability();
        });
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

    const auto camera = resourcePool()->getResourceById<QnVirtualCameraResource>(uuid);
    m_camera = camera && camera->hasTwoWayAudio() ? camera : QnVirtualCameraResourcePtr();
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
            if (!m_camera )
                return false;

            const auto user = commonModule()->instance<UserWatcher>()->user();
            if (!user)
                return false;

            const auto manager = globalPermissionsManager();
            if (!manager->hasGlobalPermission(user, GlobalPermission::userInput))
                return false;

            if (m_camera->getStatus() != Qn::Online && m_camera->getStatus() != Qn::Recording)
                return false;

            if (m_licenseHelper)
                return m_licenseHelper->status() == QnLicenseUsageStatus::used;


            return true;
        }();

    setAvailable(isAvailable);
}

void TwoWayAudioAvailabilityWatcher::setAvailable(bool value)
{
    if (m_available == value)
        return;

    m_available = value;
    emit availabilityChanged();
}

} // namespace nx::vms::client::core

