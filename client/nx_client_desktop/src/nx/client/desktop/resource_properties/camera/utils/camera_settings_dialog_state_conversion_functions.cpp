#include "camera_settings_dialog_state_conversion_functions.h"

#include <core/resource/resource_display_info.h>
#include <core/resource/camera_resource.h>

#include "../redux/camera_settings_dialog_state.h"

namespace nx {
namespace client {
namespace desktop {

namespace {

using State = CameraSettingsDialogState;
using Cameras = QnVirtualCameraResourceList;

void setMinRecordingDays(
    const State::RecordingDays& value,
    const Cameras& cameras)
{
    if (!value.same)
        return;
    int actualValue = value.absoluteValue;
    NX_ASSERT(actualValue > 0);
    if (actualValue == 0)
        actualValue = ec2::kDefaultMinArchiveDays;
    if (value.automatic)
        actualValue = -actualValue;

    for (const auto& camera: cameras)
        camera->setMinDays(actualValue);
}

void setMaxRecordingDays(
    const State::RecordingDays& value,
    const Cameras& cameras)
{
    if (!value.same)
        return;
    int actualValue = value.absoluteValue;
    NX_ASSERT(actualValue > 0);
    if (actualValue == 0)
        actualValue = ec2::kDefaultMaxArchiveDays;
    if (value.automatic)
        actualValue = -actualValue;

    for (const auto& camera: cameras)
        camera->setMaxDays(actualValue);
}

void setCustomRotation(
    const Rotation& value,
    const Cameras& cameras)
{
    for (const auto& camera: cameras)
    {
        NX_EXPECT(camera->hasVideo());
        if (!camera->hasVideo())
            continue;

        const QString rotationString = value.isValid() ? value.toString() : QString();
        camera->setProperty(QnMediaResource::rotationKey(), rotationString);
    }
}

void setCustomAspectRatio(
    const QnAspectRatio& value,
    const Cameras& cameras)
{
    for (const auto& camera: cameras)
    {
        NX_EXPECT(camera->hasVideo() && !camera->hasFlags(Qn::wearable_camera));
        if (camera->hasVideo() && !camera->hasFlags(Qn::wearable_camera))
        {
            if (value.isValid())
                camera->setCustomAspectRatio(value);
            else
                camera->clearCustomAspectRatio();
        }
    }
}

void setSchedule(const ScheduleTasks& schedule, const Cameras& cameras)
{
    QnScheduleTaskList scheduleTasks;
    for (const auto& data: schedule)
        scheduleTasks << data;

    for (const auto& camera: cameras)
    {
        if (camera->isDtsBased())
            continue;

        camera->setScheduleTasks(scheduleTasks);
    }
}

void setRecordingBeforeThreshold(
    int value,
    const Cameras& cameras)
{
    for (const auto& camera: cameras)
         camera->setRecordBeforeMotionSec(value);
}

void setRecordingAfterThreshold(
    int value,
    const Cameras& cameras)
{
    for (const auto& camera: cameras)
         camera->setRecordAfterMotionSec(value);
}


void setRecordingEnabled(bool value, const Cameras& cameras)
{
    for (const auto& camera: cameras)
        camera->setLicenseUsed(value);
}

} // namespace

void CameraSettingsDialogStateConversionFunctions::applyStateToCameras(
    const CameraSettingsDialogState& state,
    const Cameras& cameras)
{
    if (state.isSingleCamera())
    {
        cameras.first()->setName(state.singleCameraSettings.name());
    }
    setMinRecordingDays(state.recording.minDays, cameras);
    setMaxRecordingDays(state.recording.maxDays, cameras);

    if (state.imageControl.aspectRatio.hasValue())
        setCustomAspectRatio(state.imageControl.aspectRatio(), cameras);

    if (state.imageControl.rotation.hasValue())
        setCustomRotation(state.imageControl.rotation(), cameras);

    if (state.recording.enabled.hasValue())
        setRecordingEnabled(state.recording.enabled(), cameras);

    if (state.recording.schedule.hasValue())
        setSchedule(state.recording.schedule(), cameras);

    if (state.recording.thresholds.beforeSec.hasValue())
        setRecordingBeforeThreshold(state.recording.thresholds.beforeSec(), cameras);

    if (state.recording.thresholds.afterSec.hasValue())
        setRecordingAfterThreshold(state.recording.thresholds.afterSec(), cameras);
}

} // namespace desktop
} // namespace client
} // namespace nx
