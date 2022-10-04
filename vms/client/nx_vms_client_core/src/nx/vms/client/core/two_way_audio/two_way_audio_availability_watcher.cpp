// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "two_way_audio_availability_watcher.h"

#include <core/resource/camera_resource.h>
#include <core/resource_access/global_permissions_manager.h>
#include <core/resource_access/resource_access_subject.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/client/core/system_context.h>
#include <nx/vms/client/core/watchers/user_watcher.h>
#include <nx/vms/license/usage_helper.h>

namespace nx::vms::client::core {

TwoWayAudioAvailabilityWatcher::TwoWayAudioAvailabilityWatcher(QObject* parent):
    QObject(parent)
{
}

TwoWayAudioAvailabilityWatcher::~TwoWayAudioAvailabilityWatcher()
{
}

void TwoWayAudioAvailabilityWatcher::setCamera(const QnVirtualCameraResourcePtr& camera)
{
    using namespace nx::vms::license;

    if (m_camera == camera)
        return;

    m_connections.reset();

    m_camera = camera && camera->hasTwoWayAudio() ? camera : QnVirtualCameraResourcePtr();
    m_licenseHelper.reset(m_camera && m_camera->isIOModule()
        ? new SingleCamLicenseStatusHelper(m_camera)
        : nullptr);

    if (m_camera)
    {
        m_connections << connect(m_camera.get(),
            &QnVirtualCameraResource::statusChanged,
            this,
            &TwoWayAudioAvailabilityWatcher::updateAvailability);
        m_connections << connect(m_camera.get(),
            &QnSecurityCamResource::twoWayAudioEnabledChanged,
            this,
            &TwoWayAudioAvailabilityWatcher::updateAvailability);
        m_connections << connect(m_camera.get(), &QnSecurityCamResource::audioOutputDeviceIdChanged,
            this,
            &TwoWayAudioAvailabilityWatcher::updateAvailability);

        if (m_licenseHelper)
        {
            m_connections << connect(m_licenseHelper.get(),
                &SingleCamLicenseStatusHelper::licenseStatusChanged,
                this,
                &TwoWayAudioAvailabilityWatcher::updateAvailability);
        }

        auto systemContext = SystemContext::fromResource(m_camera);
        if (NX_ASSERT(systemContext))
        {
            const auto manager = systemContext->globalPermissionsManager();
            const UserWatcher* userWatcher = systemContext->userWatcher();

            m_connections << connect(userWatcher,
                &UserWatcher::userChanged,
                this,
                &TwoWayAudioAvailabilityWatcher::updateAvailability);
            m_connections << connect(manager,
                &QnGlobalPermissionsManager::globalPermissionsChanged,
                this,
                [this, userWatcher]
                    (const QnResourceAccessSubject& subject, GlobalPermissions /*permissions*/)
                {
                    const auto user = userWatcher->user();
                    if (subject != user)
                        return;

                    updateAvailability();
                });
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

            if (!m_camera->isTwoWayAudioEnabled())
                return false;

            const auto audioOutput = m_camera->audioOutputDevice();
            if (!audioOutput->hasTwoWayAudio())
                return false;

            auto systemContext = SystemContext::fromResource(m_camera);
            if (!NX_ASSERT(systemContext))
                return false;

            const auto userWatcher = systemContext->userWatcher();
            const auto user = userWatcher->user();
            if (!user)
                return false;

            const auto manager = systemContext->globalPermissionsManager();
            if (!manager->hasGlobalPermission(user, GlobalPermission::userInput))
                return false;

            if (!m_camera->isOnline())
                return false;

            if (m_licenseHelper)
                return m_licenseHelper->status() == nx::vms::license::UsageStatus::used;

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

