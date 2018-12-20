#pragma once

#include <core/resource/resource_fwd.h>

#include "camera_settings_dialog_state.h"

namespace nx::vms::client::desktop {

class NX_VMS_DESKTOP_CLIENT_API CameraSettingsDialogStateReducer
{
public:
    using State = CameraSettingsDialogState;
    using ScheduleTasks = State::RecordingSettings::ScheduleTasks;

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
    static State setPreferredPtzPresetType(State state, nx::core::ptz::PresetType value);
    static State setForcedPtzPanTiltCapability(State state, bool value);
    static State setForcedPtzZoomCapability(State state, bool value);
    static State setRtpTransportType(State state, vms::api::RtpTransportType value);
    static State setMotionStreamType(State state, vms::api::MotionStreamType value);
    static State setCustomMediaPortUsed(State state, bool value);
    static State setCustomMediaPort(State state, int value);
    static State setTrustCameraTime(State state, bool value);
    static State setLogicalId(State state, int value);
    static State generateLogicalId(State state);
    static State resetExpertSettings(State state);
    static State setAnalyticsEngines(
        State state, const QList<AnalyticsEngineInfo>& value);
    static State setCurrentAnalyticsEngineId(State state, const QnUuid& engineId);
    static State setAnalyticsSettingsLoading(State state, bool value);
    static State setEnabledAnalyticsEngines(State state, const QSet<QnUuid>& value);
    static std::pair<bool, State> setDeviceAgentSettingsValues(
        State state, const QnUuid& engineId, const QVariantMap& values);
    static std::pair<bool, State> resetDeviceAgentSettingsValues(
        State state, const QnUuid& engineId, const QVariantMap& values);
    static State setWearableMotionDetectionEnabled(State state, bool value);
    static State setWearableMotionSensitivity(State state, int value);
    static State setCredentials(
        State state, const std::optional<QString>& login, const std::optional<QString>& password);
};

} // namespace nx::vms::client::desktop
