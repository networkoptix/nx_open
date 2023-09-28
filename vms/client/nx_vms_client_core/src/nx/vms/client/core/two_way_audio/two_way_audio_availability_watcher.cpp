// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "two_way_audio_availability_watcher.h"

#include <core/resource/camera_resource.h>
#include <core/resource_access/resource_access_manager.h>
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
    nx::utils::ScopedConnections sourceConnections;
    nx::utils::ScopedConnections audioOutputConnections;
};

void TwoWayAudioAvailabilityWatcher::Private::updateAudioOutput()
{
    using namespace nx::vms::license;

    const auto camera =
        [this]() -> QnVirtualCameraResourcePtr
        {
            if (!sourceCamera)
                return {};

            const auto systemContext = SystemContext::fromResource(sourceCamera);
            const auto id = sourceCamera->audioOutputDeviceId();
            const auto result =
                systemContext->resourcePool()->getResourceById<QnVirtualCameraResource>(id);
            return result && result->hasTwoWayAudio()
                ? result
                : sourceCamera;
        }();

    if (camera == audioOutput)
        return;

    if (audioOutput && audioOutput != sourceCamera)
        audioOutputConnections.release();

    audioOutput = camera;
    helper.reset(audioOutput && audioOutput->isIOModule()
        ? new SingleCamLicenseStatusHelper(audioOutput)
        : nullptr);

    if (audioOutput)
    {
        const auto update = [this]() { updateAvailability(); };
        audioOutputConnections.add(
            connect(audioOutput.get(), &QnVirtualCameraResource::statusChanged, q, update));

        if (helper)
            connect(helper.get(), &SingleCamLicenseStatusHelper::licenseStatusChanged, q, update);
    }

    updateAvailability();
    emit q->audioOutputDeviceChanged();
}

void TwoWayAudioAvailabilityWatcher::Private::updateAvailability()
{
    const bool isAvailable =
        [this]()
        {
            if (!audioOutput || !sourceCamera)
                return false;

            if (!sourceCamera->isTwoWayAudioEnabled() || !audioOutput->hasTwoWayAudio())
                return false;

            auto systemContext = SystemContext::fromResource(sourceCamera);
            const auto user = systemContext->userWatcher()->user();
            if (!user)
                return false;

            if (!systemContext->resourceAccessManager()->hasPermission(
                user, sourceCamera, Qn::TwoWayAudioPermission))
            {
                return false;
            }

            if (!sourceCamera->isOnline() || !audioOutput->isOnline())
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

bool TwoWayAudioAvailabilityWatcher::available() const
{
    return d->available;
}

QnVirtualCameraResourcePtr TwoWayAudioAvailabilityWatcher::camera() const
{
    return d->sourceCamera;
}

void TwoWayAudioAvailabilityWatcher::setCamera(const QnVirtualCameraResourcePtr& value)
{
    if (d->sourceCamera == value)
        return;

    if (d->sourceCamera)
        d->sourceConnections.release();

    d->sourceCamera = value;

    if (d->sourceCamera)
    {
        const auto updateAvailability = [this]() { d->updateAvailability(); };
        d->sourceConnections.add(
            connect(d->sourceCamera.get(), &QnVirtualCameraResource::statusChanged,
                this, updateAvailability));
        d->sourceConnections.add(
            connect(d->sourceCamera.get(), &QnSecurityCamResource::twoWayAudioEnabledChanged,
                this, updateAvailability));
        d->sourceConnections.add(
            connect(d->sourceCamera.get(), &QnVirtualCameraResource::audioOutputDeviceIdChanged,
                this, [this]() { d->updateAudioOutput(); }));
    }

    d->updateAudioOutput();
    emit cameraChanged();
}

QnVirtualCameraResourcePtr TwoWayAudioAvailabilityWatcher::audioOutputDevice()
{
    return d->audioOutput;
}

} // namespace nx::vms::client::core
