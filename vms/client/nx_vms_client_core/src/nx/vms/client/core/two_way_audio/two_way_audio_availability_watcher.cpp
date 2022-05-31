// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "two_way_audio_availability_watcher.h"

#include <client_core/client_core_module.h>
#include <common/common_module.h>
#include <core/resource/camera_resource.h>
#include <core/resource_access/global_permissions_manager.h>
#include <core/resource_access/resource_access_subject.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/client/core/system_context.h>
#include <nx/vms/client/core/watchers/user_watcher.h>
#include <nx/vms/license/usage_helper.h>

namespace nx::vms::client::core {

TwoWayAudioAvailabilityWatcher::TwoWayAudioAvailabilityWatcher(QObject* parent):
    base_type(parent)
{
    const auto manager = globalPermissionsManager();
    const UserWatcher* userWatcher = dynamic_cast<SystemContext*>(systemContext())->userWatcher();

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
    using namespace nx::vms::license;

    if (m_camera && m_camera->getId() == uuid)
        return;

    if (m_camera)
        m_camera->disconnect(this);

    const auto camera = resourcePool()->getResourceById<QnVirtualCameraResource>(uuid);
    m_camera = camera && camera->hasTwoWayAudio() ? camera : QnVirtualCameraResourcePtr();
    m_licenseHelper.reset(m_camera && m_camera->isIOModule()
        ? new SingleCamLicenseStatusHelper(m_camera)
        : nullptr);

    if (m_camera)
    {
        connect(m_camera, &QnVirtualCameraResource::statusChanged,
            this, &TwoWayAudioAvailabilityWatcher::updateAvailability);
        connect(m_camera, &QnSecurityCamResource::twoWayAudioEnabledChanged,
            this, &TwoWayAudioAvailabilityWatcher::updateAvailability);
        connect(m_camera, &QnSecurityCamResource::audioOutputDeviceIdChanged,
            this, &TwoWayAudioAvailabilityWatcher::updateAvailability);

        if (m_licenseHelper)
        {
            connect(m_licenseHelper, &SingleCamLicenseStatusHelper::licenseStatusChanged,
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

            if (!m_camera->isTwoWayAudioEnabled())
                return false;

            const auto audioOutput = m_camera->audioOutputDevice();
            if (!audioOutput->hasTwoWayAudio())
                return false;

            const auto userWatcher = dynamic_cast<SystemContext*>(systemContext())->userWatcher();
            const auto user = userWatcher->user();
            if (!user)
                return false;

            const auto manager = globalPermissionsManager();
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

