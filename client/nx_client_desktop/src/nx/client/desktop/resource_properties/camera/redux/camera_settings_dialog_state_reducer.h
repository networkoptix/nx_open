#pragma once

#include <core/resource/resource_fwd.h>

#include "camera_settings_dialog_state.h"

namespace nx {
namespace client {
namespace desktop {

class CameraSettingsDialogStateReducer
{
public:
    using State = CameraSettingsDialogState;

    static State applyChanges(State state);
    static State setReadOnly(State state, bool value);
    static State setSettingsOptimizationEnabled(State state, bool value);
    static State setGlobalPermissions(State state, GlobalPermissions value);
    static State setSingleWearableState(State state, const WearableState& value);

    static State loadCameras(State state, const QnVirtualCameraResourceList& cameras);

    static State setSingleCameraUserName(State state, const QString& text);
    static State setScheduleBrush(State state, const ScheduleCellParams& brush);
    static State setScheduleBrushRecordingType(State state, Qn::RecordingType value);
    static State setScheduleBrushFps(State state, int value);
    static State setScheduleBrushQuality(State state, Qn::StreamQuality value);
    static State setSchedule(State state, const ScheduleTasks& schedule);
    static State setRecordingShowFps(State state, bool value);
    static State setRecordingShowQuality(State state, bool value);
    static State toggleCustomBitrateVisible(State state);
    static State setCustomRecordingBitrateMbps(State state, float mbps);
    static State setCustomRecordingBitrateNormalized(State state, float value);
    static State setMinRecordingDaysAutomatic(State state, bool value);
    static State setMinRecordingDaysValue(State state, int value);
    static State setMaxRecordingDaysAutomatic(State state, bool value);
    static State setMaxRecordingDaysValue(State state, int value);
    static State setRecordingBeforeThresholdSec(State state, int value);
    static State setRecordingAfterThresholdSec(State state, int value);
    static State setCustomAspectRatio(State state, const QnAspectRatio& value);
    static State setCustomRotation(State state, const Rotation& value);
    static State setRecordingEnabled(State state, bool value);
    static State setAudioEnabled(State state, bool value);
    static State setMotionDetectionEnabled(State state, bool value);
    static State setMotionRegionList(State state, const QList<QnMotionRegion>& value);
    static State setFisheyeSettings(State state, const QnMediaDewarpingParams& value);
    static State setIoPortDataList(State state, const QnIOPortDataList& value);
    static State setIoModuleVisualStyle(State state, vms::api::IoModuleVisualStyle value);
    static State setCameraControlDisabled(State state, bool value);
    static State setDualStreamingDisabled(State state, bool value);
    static State setUseBitratePerGOP(State state, bool value);
    static State setPrimaryRecordingDisabled(State state, bool value);
    static State setSecondaryRecordingDisabled(State state, bool value);
    static State setNativePtzPresetsDisabled(State state, bool value);
    static State setRtpTransportType(State state, vms::api::RtpTransportType value);
    static State setMotionStreamType(State state, vms::api::MotionStreamType value);
    static State setCustomMediaPortUsed(State state, bool value);
    static State setCustomMediaPort(State state, int value);
    static State setLogicalId(State state, int value);
    static State generateLogicalId(State state);
    static State resetExpertSettings(State state);
    static State setWearableMotionDetectionEnabled(State state, bool value);
    static State setWearableMotionSensitivity(State state, int value);
    static State setCredentials(
        State state, const std::optional<QString>& login, const std::optional<QString>& password);
};

} // namespace desktop
} // namespace client
} // namespace nx