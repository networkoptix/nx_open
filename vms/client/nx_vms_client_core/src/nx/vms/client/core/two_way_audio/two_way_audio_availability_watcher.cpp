// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "two_way_audio_availability_watcher.h"

#include <core/resource/camera_resource.h>
#include <core/resource_access/global_permissions_manager.h>
#include <core/resource_access/resource_access_subject.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/scoped_connections.h>
#include <nx/vms/client/core/system_context.h>
#include <nx/vms/client/core/watchers/user_watcher.h>
#include <nx/vms/license/usage_helper.h>

namespace nx::vms::client::core {

struct TwoWayAudioAvailabilityWatcher::Private
{
    void updateAudioOutput();
    void updateAvailability();
    void setAvailable(bool value);

    TwoWayAudioAvailabilityWatcher* const q;
    bool available = false;
    QnVirtualCameraResourcePtr sourceCamera;
    QnVirtualCameraResourcePtr audioOutput;
    std::unique_ptr<nx::vms::license::SingleCamLicenseStatusHelper> helper;
    nx::utils::ScopedConnections connections;
};

void TwoWayAudioAvailabilityWatcher::Private::updateAudioOutput()
{
    const auto camera = (sourceCamera && sourceCamera->audioOutputDevice()->hasTwoWayAudio()
        ? sourceCamera->audioOutputDevice()
        : QnSecurityCamResourcePtr()).dynamicCast<QnVirtualCameraResource>();

    if (camera == audioOutput)
        return;

    audioOutput = camera;

    helper.reset(audioOutput && audioOutput->isIOModule()
        ? new license::SingleCamLicenseStatusHelper(audioOutput)
        : nullptr);

    if (audioOutput)
    {
        const auto updateAvailabilityHandler = [this]() { updateAvailability(); };

        connections << connect(audioOutput.get(),
            &QnVirtualCameraResource::statusChanged,
            q,
            updateAvailabilityHandler);
        connections << connect(audioOutput.get(),
            &QnSecurityCamResource::twoWayAudioEnabledChanged,
            q,
            updateAvailabilityHandler);
        connections << connect(audioOutput.get(),
            &QnSecurityCamResource::audioOutputDeviceIdChanged,
            q,
            updateAvailabilityHandler);

        if (helper)
        {
            connections << connect(helper.get(),
                &license::SingleCamLicenseStatusHelper::licenseStatusChanged,
                q,
                updateAvailabilityHandler);
        }

        auto systemContext = SystemContext::fromResource(audioOutput);
        if (NX_ASSERT(systemContext))
        {
            const auto manager = systemContext->globalPermissionsManager();
            const UserWatcher* userWatcher = systemContext->userWatcher();

            connections << connect(userWatcher,
                &UserWatcher::userChanged,
                q,
                updateAvailabilityHandler);

            const auto handlePermissionChanged =
                [this, userWatcher]
                   (const QnResourceAccessSubject& subject, GlobalPermissions /*permissions*/)
                {
                    const auto user = userWatcher->user();
                    if (subject != user)
                        return;

                    updateAvailability();
                };
            connections << connect(manager,
                &QnGlobalPermissionsManager::globalPermissionsChanged,
                q,
                handlePermissionChanged);
        }
    }

    updateAvailability();
    q->audioOutputDeviceChanged();
}

void TwoWayAudioAvailabilityWatcher::Private::updateAvailability()
{
    const bool isAvailable =
        [this]()
        {
            if (!audioOutput )
                return false;

            if (!audioOutput->isTwoWayAudioEnabled() || !audioOutput->hasTwoWayAudio())
                return false;

            auto systemContext = SystemContext::fromResource(audioOutput);
            if (!NX_ASSERT(systemContext))
                return false;

            const auto userWatcher = systemContext->userWatcher();
            const auto user = userWatcher->user();
            if (!user)
                return false;

            const auto manager = systemContext->globalPermissionsManager();
            if (!manager->hasGlobalPermission(user, GlobalPermission::userInput))
                return false;

            if (!audioOutput->isOnline())
                return false;

            if (helper)
                return helper->status() == nx::vms::license::UsageStatus::used;

            return true;
        }();

    setAvailable(isAvailable);
}

void TwoWayAudioAvailabilityWatcher::Private::setAvailable(bool value)
{
    if (available == value)
        return;

    available = value;
    emit q->availabilityChanged();
}

//-------------------------------------------------------------------------------------------------

TwoWayAudioAvailabilityWatcher::TwoWayAudioAvailabilityWatcher(QObject* parent):
    QObject(parent),
    d(new Private{.q = this})
{
}

TwoWayAudioAvailabilityWatcher::~TwoWayAudioAvailabilityWatcher()
{
}

QnVirtualCameraResourcePtr TwoWayAudioAvailabilityWatcher::camera() const
{
    return d->sourceCamera;
}

void TwoWayAudioAvailabilityWatcher::setCamera(const QnVirtualCameraResourcePtr& camera)
{
    if (d->sourceCamera == camera)
        return;

    d->connections.reset();

    d->sourceCamera = camera;

    if (d->sourceCamera)
    {
        connect(d->sourceCamera.get(), &QnVirtualCameraResource::audioOutputDeviceIdChanged,
            this, [this]() { d->updateAudioOutput(); });
    }

    d->updateAudioOutput();
    emit cameraChanged();
}

bool TwoWayAudioAvailabilityWatcher::available() const
{
    return d->available;
}

QnVirtualCameraResourcePtr TwoWayAudioAvailabilityWatcher::audioOutputDevice()
{
    return d->audioOutput;
}

} // namespace nx::vms::client::core

