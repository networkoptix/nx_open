// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QJsonObject>
#include <QtCore/QList>
#include <QtCore/QObject>

#include <api/model/api_ioport_data.h>
#include <common/common_globals.h>
#include <core/ptz/ptz_preset.h>
#include <core/resource/motion_window.h>
#include <core/resource/resource_fwd.h>
#include <nx/core/resource/using_media2_type.h>
#include <nx/utils/std/optional.h>
#include <nx/vms/api/data/saas_data.h>
#include <nx/vms/api/types/rtp_types.h>
#include <nx/vms/client/core/resource/media_dewarping_params.h>
#include <nx/vms/client/desktop/common/flux/flux_types.h>

#include "camera_settings_dialog_state.h"

class QnAspectRatio;
struct QnCameraAdvancedParams;

namespace nx::vms::api::dewarping { struct MediaData; }
namespace nx::vms::client::core { enum class StandardRotation; }

namespace nx::vms::client::desktop {

enum class CameraSettingsTab;
struct VirtualCameraState;
struct RecordScheduleCellData;
struct AnalyticsEngineInfo;
struct DeviceAgentData;
class DeviceAgentSettingsAdapter;
class CameraSettingsAnalyticsEnginesWatcherInterface;
class CameraAdvancedParametersManifestManager;

class NX_VMS_CLIENT_DESKTOP_API CameraSettingsDialogStore: public QObject
{
    Q_OBJECT
    using base_type = QObject;

public:
    explicit CameraSettingsDialogStore(QObject* parent = nullptr);
    virtual ~CameraSettingsDialogStore() override;

    const CameraSettingsDialogState& state() const;

    /** Initial dialog opening or cameras set change. */
    void loadCameras(
        const QnVirtualCameraResourceList& cameras,
        DeviceAgentSettingsAdapter* deviceAgentSettingsAdapter = nullptr,
        CameraSettingsAnalyticsEnginesWatcherInterface* analyticsEnginesWatcher = nullptr,
        CameraAdvancedParametersManifestManager* advancedParametersManifestManager = nullptr);

    /** Motion detection support was changed on one of the cameras. */
    void handleMotionTypeChanged(const QnVirtualCameraResourceList& cameras);

    /** Motion detection was forced on one of the cameras. */
    void handleMotionForcedChanged(const QnVirtualCameraResourceList& cameras);

    /** Dual streaming support was changed on one of the cameras. */
    void handleDualStreamingChanged(const QnVirtualCameraResourceList& cameras);

    /** Media streams parameters were changed on one of the cameras. */
    void handleMediaStreamsChanged(const QnVirtualCameraResourceList& cameras);

    /** Motion stream was changed on one of the cameras. */
    void handleMotionStreamChanged(const QnVirtualCameraResourceList& cameras);

    /** Motion regions were changed on one of the cameras. */
    void handleMotionRegionsChanged(const QnVirtualCameraResourceList& cameras);

    /** Stream urls were changed on one of the cameras. */
    void handleStreamUrlsChanged(const QnVirtualCameraResourceList& cameras);

    /** Compatible object types were changed on one of the cameras. */
    void handleCompatibleObjectTypesMaybeChanged(const QnVirtualCameraResourceList& cameras);

    /** Status of one of the cameras was changed. */
    void handleStatusChanged(const QnVirtualCameraResourceList& cameras);

    /** Media capabilities were changed on one of the cameras. */
    void handleMediaCapabilitiesChanged(const QnVirtualCameraResourceList& cameras);

    /** Audio enabled status were changed on one of the cameras. */
    void handleAudioEnabledChanged(const QnVirtualCameraResourceList& cameras);

    void updatePtzSettings(const QnVirtualCameraResourceList& cameras);
    void setReadOnly(bool value);
    void setSelectedTab(CameraSettingsTab value);
    void setSettingsOptimizationEnabled(bool value);
    void setHasPowerUserPermissions(bool value);
    void setHasEventLogPermission(bool value);
    void setHasEditAccessRightsForAllCameras(bool value);
    void setPermissions(Qn::Permissions value);
    void setSaasInitialized(bool value);
    void setSingleVirtualCameraState(const VirtualCameraState& value);
    void setSingleCameraUserName(const QString& text);
    void setSingleCameraIsOnline(bool isOnline);
    void setScheduleBrush(const RecordScheduleCellData& brush);
    void setScheduleBrushRecordingType(Qn::RecordingType value);
    void setRecordingMetadataRadioButton(CameraSettingsDialogState::MetadataRadioButton value);
    void setScheduleBrushFps(int value);
    void setScheduleBrushQuality(Qn::StreamQuality value);
    void setSchedule(const QnScheduleTaskList& schedule);

    /** User clicked on "Fixup Schedule" link on schedule alerts banner. */
    void fixupSchedule();

    void setRecordingShowFps(bool value);
    void setRecordingShowQuality(bool value);
    void toggleCustomBitrateVisible();
    void setRecordingBitrateMbps(float mbps);
    void setRecordingBitrateNormalized(float value);
    void setMinRecordingPeriodAutomatic(bool value);
    void setMinRecordingPeriodValue(std::chrono::seconds value);
    void setMaxRecordingPeriodAutomatic(bool value);
    void setMaxRecordingPeriodValue(std::chrono::seconds value);
    void setRecordingBeforeThresholdSec(int value);
    void setRecordingAfterThresholdSec(int value);
    void setCustomAspectRatio(const QnAspectRatio& value);
    void setCustomRotation(core::StandardRotation value);
    void setRecordingEnabled(bool value);
    void setAudioEnabled(bool value);
    void setAudioInputDeviceId(const QnUuid& deviceId);
    void setTwoWayAudioEnabled(bool value);
    void setAudioOutputDeviceId(const QnUuid& deviceId);
    void setCameraHotspotsEnabled(bool value);
    void setCameraHotspotsData(const nx::vms::common::CameraHotspotDataList& cameraHotspots);

    /** User clicked on "Motion detection" checkbox. */
    void setMotionDetectionEnabled(bool value);

    /** User forced motion detection using "Expert settings" button. */
    void forceMotionDetection();

    /** User manually selected motion stream. */
    void setMotionStream(nx::vms::api::StreamIndex value);

    void setCurrentMotionSensitivity(int value);

    /** User manually changed motion regions. */
    void setMotionRegionList(const QList<QnMotionRegion>& value);

    void setMotionDependingOnDualStreaming(CombinedValue value);
    void setIoPortDataList(const QnIOPortDataList& value);
    void setIoModuleVisualStyle(nx::vms::api::IoModuleVisualStyle value);
    void setCameraControlDisabled(bool value);
    void setKeepCameraTimeSettings(bool value);

    /** User enabled or disabled dual streaming using "Expert settings". */
    void setDualStreamingDisabled(bool value);

    void setUseBitratePerGOP(bool value);
    void setUseMedia2ToFetchProfiles(nx::core::resource::UsingOnvifMedia2Type value);
    void setPrimaryRecordingDisabled(bool value);
    void setRecordAudioEnabled(bool value);
    void setSecondaryRecordingDisabled(bool value);
    void setPreferredPtzPresetType(nx::core::ptz::PresetType value);
    void setForcedPtzPanTiltCapability(bool value);
    void setForcedPtzZoomCapability(bool value);
    void setDifferentPtzPanTiltSensitivities(bool value);

    /**
     * Enable or disable motion detection in the remote archive. Actual for the Edge Cameras (with
     * RemoteArchiveCapability option).
     */
    void setRemoteArchiveMotionDetectionEnabled(bool value);

    void setPtzPanSensitivity(qreal value);
    void setPtzTiltSensitivity(qreal value);
    void setRtpTransportType(nx::vms::api::RtpTransportType value);
    void setForcedPrimaryProfile(const QString& value);
    void setForcedSecondaryProfile(const QString& value);
    void setRemoteArchiveSyncronizationEnabled(bool value);
    void setAutoMediaPortUsed(bool value);
    void setTrustCameraTime(bool value);
    void setCustomMediaPort(int value);
    void setCustomWebPagePort(int value);
    void setLogicalId(int value);
    void generateLogicalId();
    void resetExpertSettings();

    Q_INVOKABLE QnUuid resourceId() const;
    Q_INVOKABLE QVariantList analyticsEngines() const;
    void setAnalyticsEngines(const QList<AnalyticsEngineInfo>& value);
    Q_INVOKABLE QVariantList userEnabledAnalyticsEngines() const;
    void setUserEnabledAnalyticsEngines(const QSet<QnUuid>& value);
    Q_INVOKABLE QnUuid currentAnalyticsEngineId() const;
    Q_INVOKABLE void setCurrentAnalyticsEngineId(const QnUuid& engineId);
    Q_INVOKABLE bool analyticsSettingsLoading() const;
    Q_INVOKABLE void setUserEnabledAnalyticsEngines(const QVariantList& value);
    Q_INVOKABLE int analyticsStreamIndex(const QnUuid& engineId) const;
    Q_INVOKABLE void setAnalyticsStreamIndex(const QnUuid& engineId, int value);
    void setAnalyticsStreamIndex(const QnUuid& engineId,
        nx::vms::api::StreamIndex value,
        ModificationSource source = ModificationSource::local);

    Q_INVOKABLE QJsonObject deviceAgentSettingsModel(const QnUuid& engineId) const;
    Q_INVOKABLE QJsonObject deviceAgentSettingsValues(const QnUuid& engineId) const;
    Q_INVOKABLE void setDeviceAgentSettingsValues(
        const QnUuid& engineId,
        const QString& activeElement,
        const QJsonObject& paramsModel,
        const QJsonObject& values);
    void handleOverusedEngines(const QSet<QnUuid>& overusedEngines);

    Q_INVOKABLE void refreshDeviceAgentSettings(const QnUuid& engineId);
    void resetDeviceAgentData(
        const QnUuid& engineId, const DeviceAgentData& data, bool resetUser = true);
    Q_INVOKABLE QJsonObject deviceAgentSettingsErrors(const QnUuid& engineId) const;

    Q_INVOKABLE bool dualStreamingEnabled() const;
    Q_INVOKABLE bool recordingEnabled() const;

    void setDewarpingParams(const nx::vms::api::dewarping::MediaData& value);

    Q_INVOKABLE nx::vms::client::core::MediaDewarpingParams dewarpingParams() const;
    Q_INVOKABLE void setDewarpingParams(
        bool enabled,
        qreal centerX,
        qreal centerY,
        qreal radius,
        qreal stretch,
        qreal rollCorrectionDegrees,
        nx::vms::api::dewarping::FisheyeCameraMount cameraMount,
        nx::vms::api::dewarping::CameraProjection cameraProjection,
        qreal sphereAlpha,
        qreal sphereBeta);

    void setVirtualCameraIgnoreTimeZone(bool value);
    void setVirtualCameraMotionDetectionEnabled(bool value);
    void setVirtualCameraMotionSensitivity(int value);
    void setCredentials(
        const std::optional<QString>& login, const std::optional<QString>& password);

    /** User manually edited stream urls. */
    void setStreamUrls(const QString& primary, const QString& secondary);

    void setAdvancedSettingsManifest(const QnCameraAdvancedParams& manifest);
    void setHasExportPermission(bool value);
    void setScreenRecordingOn(bool value);

    Q_INVOKABLE bool isReadOnly() const;

signals:
    void stateChanged(const CameraSettingsDialogState& state);
    void stateModified(); //< Same as stateChanged() but without parameters.

private:
    struct Private;
    QScopedPointer<Private> d;
};

} // namespace nx::vms::client::desktop
