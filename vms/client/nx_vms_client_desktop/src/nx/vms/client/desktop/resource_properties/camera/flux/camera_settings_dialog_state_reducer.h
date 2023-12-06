// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <api/model/analytics_actions.h>
#include <core/resource/resource_fwd.h>

#include "camera_settings_dialog_state.h"

namespace nx::vms::client::desktop {

class DeviceAgentSettingsAdapter;
class CameraSettingsAnalyticsEnginesWatcherInterface;
class CameraAdvancedParametersManifestManager;

class NX_VMS_CLIENT_DESKTOP_API CameraSettingsDialogStateReducer
{
public:
    using State = CameraSettingsDialogState;
    using ScheduleTasks = State::RecordingSettings::ScheduleTasks;

    static State loadCameras(
        State state,
        const QnVirtualCameraResourceList& cameras,
        DeviceAgentSettingsAdapter* deviceAgentSettingsAdapter = nullptr,
        CameraSettingsAnalyticsEnginesWatcherInterface* analyticsEnginesWatcher = nullptr,
        CameraAdvancedParametersManifestManager* advancedParametersManifestManager = nullptr);

    /** Motion detection support was changed on one of the cameras. */
    static State handleMotionTypeChanged(State state, const QnVirtualCameraResourceList& cameras);

    /** Motion detection was forced on one of the cameras. */
    static State handleMotionForcedChanged(
        State state,
        const QnVirtualCameraResourceList& cameras);

    /** Dual streaming support was changed on one of the cameras. */
    static State handleDualStreamingChanged(
        State state,
        const QnVirtualCameraResourceList& cameras);

    /** Media streams parameters were changed on one of the cameras. */
    static State handleMediaStreamsChanged(
        State state,
        const QnVirtualCameraResourceList& cameras);

    /** Motion stream was changed on one of the cameras. */
    static State handleMotionStreamChanged(
        State state,
        const QnVirtualCameraResourceList& cameras);

    /** Motion regions were changed on one of the cameras. */
    static State handleMotionRegionsChanged(
        State state,
        const QnVirtualCameraResourceList& cameras);

    /** Stream urls were changed on one of the cameras. */
    static State handleStreamUrlsChanged(
        State state,
        const QnVirtualCameraResourceList& cameras);

    /** Compatible object types were changed on one of the cameras. */
    static State handleCompatibleObjectTypesMaybeChanged(
        State state,
        const QnVirtualCameraResourceList& cameras);

    /** Status of one of the cameras was changed. */
    static State handleStatusChanged(State state, const QnVirtualCameraResourceList& cameras);

    /** Media capabilities were changed on one of the cameras. */
    static State handleMediaCapabilitiesChanged(
        State state,
        const QnVirtualCameraResourceList& cameras);

    /** Audio enabled status were changed on one of the cameras. */
    static State handleAudioEnabledChanged(
        State state,
        const QnVirtualCameraResourceList& cameras);

    static State updatePtzSettings(State state, const QnVirtualCameraResourceList& cameras);
    static State setSelectedTab(State state, CameraSettingsTab value);
    static State setReadOnly(State state, bool value);
    static State setSettingsOptimizationEnabled(State state, bool value);
    static State setHasPowerUserPermissions(State state, bool value);
    static State setHasEventLogPermission(State state, bool value);
    static State setHasEditAccessRightsForAllCameras(State state, bool value);
    static State setPermissions(State state, Qn::Permissions value);
    static State setSaasInitialized(State state, bool value);
    static State setSingleVirtualCameraState(State state, const VirtualCameraState& value);

    static State setSingleCameraUserName(State state, const QString& text);
    static std::pair<bool, State> setSingleCameraIsOnline(State state, bool isOnline);
    static State setScheduleBrush(State state, const RecordScheduleCellData& brush);
    static State setScheduleBrushRecordingType(State state, Qn::RecordingType value);
    static State setRecordingMetadataRadioButton(State state, State::MetadataRadioButton value);
    static State setScheduleBrushFps(State state, int value);
    static State setScheduleBrushQuality(State state, Qn::StreamQuality value);
    static State setSchedule(State state, const ScheduleTasks& schedule);

    /** User clicked on "Fixup Schedule" link on schedule alerts banner. */
    static State fixupSchedule(State state);

    static State setRecordingShowFps(State state, bool value);
    static State setRecordingShowQuality(State state, bool value);
    static State toggleCustomBitrateVisible(State state);
    static State setRecordingBitrateMbps(State state, float mbps);
    static State setRecordingBitrateNormalized(State state, float value);
    static State setMinRecordingPeriodAutomatic(State state, bool value);
    static State setMinRecordingPeriodValue(State state, std::chrono::seconds value);
    static State setMaxRecordingPeriodAutomatic(State state, bool value);
    static State setMaxRecordingPeriodValue(State state, std::chrono::seconds value);
    static State setRecordingBeforeThresholdSec(State state, int value);
    static State setRecordingAfterThresholdSec(State state, int value);
    static State setCustomAspectRatio(State state, const QnAspectRatio& value);
    static State setCustomRotation(State state, core::StandardRotation value);
    static std::pair<bool, State> setRecordingEnabled(State state, bool value);
    static State setAudioEnabled(State state, bool value);
    static State setAudioInputDeviceId(State state, const QnUuid& deviceId);
    static State setTwoWayAudioEnabled(State state, bool value);
    static State setAudioOutputDeviceId(State state, const QnUuid& deviceId);
    static State setCameraHotspotsEnabled(State state, bool value);
    static State setCameraHotspotsData(State state,
        const nx::vms::common::CameraHotspotDataList& cameraHotspots);

    /** User clicked on "Motion detection" checkbox. */
    static State setMotionDetectionEnabled(State state, bool value);

    /** User forced motion detection using "Expert settings" button. */
    static State forceMotionDetection(State state);

    /** User manually selected motion stream. */
    static State setMotionStream(State state, nx::vms::api::StreamIndex value);

    static State setCurrentMotionSensitivity(State state, int value);
    static State setMotionRegionList(State state, const QList<QnMotionRegion>& value);
    static std::pair<bool, State> setMotionDependingOnDualStreaming(State state,
        CombinedValue value);
    static State setDewarpingParams(State state, const nx::vms::api::dewarping::MediaData& value);
    static State setIoPortDataList(State state, const QnIOPortDataList& value);
    static State setIoModuleVisualStyle(State state, nx::vms::api::IoModuleVisualStyle value);
    static State setCameraControlDisabled(State state, bool value);
    static State setKeepCameraTimeSettings(State state, bool value);

    /** User enabled or disabled dual streaming using "Expert settings". */
    static State setDualStreamingDisabled(State state, bool value);

    static State setUseBitratePerGOP(State state, bool value);
    static State setUseMedia2ToFetchProfiles(State state, nx::core::resource::UsingOnvifMedia2Type value);
    static State setPrimaryRecordingDisabled(State state, bool value);
    static State setRecordAudioEnabled(State state, bool value);
    static State setSecondaryRecordingDisabled(State state, bool value);
    static State setPreferredPtzPresetType(State state, nx::core::ptz::PresetType value);
    static State setForcedPtzPanTiltCapability(State state, bool value);
    static State setForcedPtzZoomCapability(State state, bool value);
    static State setDoNotSendStopPtzCommand(State state, bool value);
    static State setRtpTransportType(State state, vms::api::RtpTransportType value);
    static State setForcedPrimaryProfile(State state, const QString& value);
    static State setForcedSecondaryProfile(State state, const QString& value);
    static State setRemoteArchiveSyncronizationEnabled(State state, bool value);
    static State setCustomMediaPortUsed(State state, bool value);
    static State setCustomMediaPort(State state, int value);
    static State setCustomWebPagePort(State state, int value);
    static State setTrustCameraTime(State state, bool value);
    static State setLogicalId(State state, int value);
    static State generateLogicalId(State state);
    static State resetExpertSettings(State state);
    static std::pair<bool, State> setAnalyticsEngines(
        State state, const QList<AnalyticsEngineInfo>& value);
    static State handleOverusedEngines(State state, const QSet<QnUuid>& overusedEngines);
    static std::pair<bool, State> setCurrentAnalyticsEngineId(State state, const QnUuid& value);

    static State setUserEnabledAnalyticsEngines(State state, const QSet<QnUuid>& value);
    static State setAnalyticsStreamIndex(
        State state, const QnUuid& engineId, State::StreamIndex value, ModificationSource source);

    static std::pair<bool, State> setDeviceAgentSettingsValues(
        State state,
        const QnUuid& engineId,
        const QString& activeElement,
        const QJsonObject& values,
        const QJsonObject& paramValues);

    static State refreshDeviceAgentSettings(State state, const QnUuid& engineId);

    /**
     * Resets Analytics Device Agent Data.
     * @param state State.
     * @param engineId Engine id.
     * @param values New Device Agent Data.
     * @param replaceUser Replaces user values if true, otherwise resets user and base values.
     * @return Updated state.
     */
    static std::pair<bool, State> resetDeviceAgentData(
        State state,
        const QnUuid& engineId,
        const DeviceAgentData& values,
        bool replaceUser = false);

    static State setVirtualCameraIgnoreTimeZone(State state, bool value);
    static State setVirtualCameraMotionDetectionEnabled(State state, bool value);
    static State setVirtualCameraMotionSensitivity(State state, int value);
    static State setCredentials(
        State state, const std::optional<QString>& login, const std::optional<QString>& password);

    /** User manually edited stream urls. */
    static State setStreamUrls(State state, const QString& primary, const QString& secondary);

    static State setAdvancedSettingsManifest(State state, const QnCameraAdvancedParams& manifest);

    static State setDifferentPtzPanTiltSensitivities(State state, bool value);
    static State setPtzPanSensitivity(State state, qreal value);
    static State setPtzTiltSensitivity(State state, qreal value);
    static State setHasExportPermission(State state, bool value);
    static State setScreenRecordingOn(State state, bool value);

    /**
     * Enable or disable motion detection in the remote archive. Actual for the Edge Cameras (with
     * RemoteArchiveCapability option).
     */
    static State setRemoteArchiveMotionDetectionEnabled(State state, bool value);

    static bool isMotionDetectionDependingOnDualStreaming(const QnVirtualCameraResourcePtr& camera);
};

} // namespace nx::vms::client::desktop
