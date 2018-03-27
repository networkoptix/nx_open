#include "camera_settings_dialog_state_conversion_functions.h"

#include <core/resource/resource_display_info.h>
#include <core/resource/camera_resource.h>

#include "../redux/camera_settings_dialog_state.h"

namespace nx {
namespace client {
namespace desktop {

namespace {

using State = CameraSettingsDialogState;

void setMinRecordingDays(
    const State::RecordingDays& value,
    const QnVirtualCameraResourceList& cameras)
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
    const QnVirtualCameraResourceList& cameras)
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
    const QnVirtualCameraResourceList& cameras)
{
    for (const auto& camera: cameras)
    {
        NX_EXPECT(camera->hasVideo());
        if (camera->hasVideo())
        {
            if (value.isValid())
                camera->setProperty(QnMediaResource::rotationKey(), value.toString());
            else
                camera->setProperty(QnMediaResource::rotationKey(), QString());
        }
    }
}

void setCustomAspectRatio(
    const QnAspectRatio& value,
    const QnVirtualCameraResourceList& cameras)
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

} // namespace


void CameraSettingsDialogStateConversionFunctions::applyStateToCameras(
    const CameraSettingsDialogState& state,
    const QnVirtualCameraResourceList& cameras)
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
}


} // namespace desktop
} // namespace client
} // namespace nx
