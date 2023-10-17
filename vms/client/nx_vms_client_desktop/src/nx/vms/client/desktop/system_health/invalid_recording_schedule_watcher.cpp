// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "invalid_recording_schedule_watcher.h"

#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/client/core/resource/session_resources_signal_listener.h>
#include <nx/vms/client/desktop/system_context.h>

namespace nx::vms::client::desktop {

namespace {

bool isScheduleValid(const QnVirtualCameraResourcePtr& camera)
{
    return !camera->isScheduleEnabled()
        || !camera->supportsSchedule()
        || camera->canApplySchedule(camera->getScheduleTasks());
}

} // namespace

struct InvalidRecordingScheduleWatcher::Private
{
    explicit Private(InvalidRecordingScheduleWatcher* owner):
        q(owner)
    {
    }

    bool addCameraIfScheduleIsInvalid(const QnVirtualCameraResourcePtr& camera)
    {
        if (isScheduleValid(camera))
            return false;

        if (camerasWithInvalidSchedule.contains(camera))
            return false;

        camerasWithInvalidSchedule.insert(camera);
        return true;
    }

    void handleCameraChanged(const QnVirtualCameraResourcePtr& camera)
    {
        bool changed = false;
        if (isScheduleValid(camera))
        {
            changed = camerasWithInvalidSchedule.remove(camera);
        }
        else
        {
            changed = !camerasWithInvalidSchedule.contains(camera);
            camerasWithInvalidSchedule.insert(camera);
        }

        if (changed)
            emit q->camerasChanged();
    }

    InvalidRecordingScheduleWatcher* const q;
    QnVirtualCameraResourceSet camerasWithInvalidSchedule;
};

InvalidRecordingScheduleWatcher::InvalidRecordingScheduleWatcher(
    SystemContext* systemContext,
    QObject* parent)
    :
    base_type(parent),
    SystemContextAware(systemContext),
    d(new Private(this))
{
    auto camerasListener = new core::SessionResourcesSignalListener<QnVirtualCameraResource>(
        systemContext,
        this);

    camerasListener->setOnAddedHandler(
        [this](const QnVirtualCameraResourceList& cameras)
        {
            bool changed = false;
            for (const auto& camera: cameras)
                changed |= d->addCameraIfScheduleIsInvalid(camera);

            if (changed)
                emit camerasChanged();
        });

    camerasListener->setOnRemovedHandler(
        [this](const QnVirtualCameraResourceList& cameras)
        {
            bool changed = false;
            for (const auto& camera: cameras)
                changed |= d->camerasWithInvalidSchedule.remove(camera);

            if (changed)
                emit camerasChanged();
        });

    auto handleCameraChanged =
        [this](const QnVirtualCameraResourcePtr& camera)
        {
            d->handleCameraChanged(camera);
        };

    camerasListener->addOnSignalHandler(&QnVirtualCameraResource::scheduleEnabledChanged,
        handleCameraChanged);
    camerasListener->addOnSignalHandler(&QnVirtualCameraResource::scheduleTasksChanged,
        handleCameraChanged);
    camerasListener->addOnSignalHandler(&QnVirtualCameraResource::statusFlagsChanged,
        handleCameraChanged);
    camerasListener->addOnSignalHandler(&QnVirtualCameraResource::disableDualStreamingChanged,
        handleCameraChanged);
    camerasListener->addOnSignalHandler(&QnVirtualCameraResource::motionTypeChanged,
        handleCameraChanged);
    camerasListener->addOnSignalHandler(
        &QnVirtualCameraResource::compatibleObjectTypesMaybeChanged,
        handleCameraChanged);

    camerasListener->addOnPropertyChangedHandler(
        [handleCameraChanged]
            (const QnVirtualCameraResourcePtr& camera, const QString& key)
        {
            if (key == ResourcePropertyKey::kStreamUrls
                || key == ResourcePropertyKey::kMediaStreams
                || key == ResourcePropertyKey::kMotionStreamKey
                || key == ResourcePropertyKey::kForcedMotionDetectionKey
                || key == ResourcePropertyKey::kDontRecordSecondaryStreamKey
                )
            {
                handleCameraChanged(camera);
            }
        });

    camerasListener->start();
}

InvalidRecordingScheduleWatcher::~InvalidRecordingScheduleWatcher()
{
}

QnVirtualCameraResourceSet InvalidRecordingScheduleWatcher::camerasWithInvalidSchedule() const
{
    return d->camerasWithInvalidSchedule;
}

} // namespace nx::vms::client::desktop
