#pragma once

#include <QtCore/QList>
#include <QtCore/QObject>

#include <api/model/api_ioport_data.h>
#include <common/common_globals.h>
#include <core/ptz/ptz_preset.h>
#include <core/resource/resource_fwd.h>
#include <core/resource/motion_window.h>

#include <nx/utils/std/optional.h>

class QnAspectRatio;
struct QnMediaDewarpingParams;

namespace nx::vms::client::desktop {

class Rotation;
struct WearableState;
struct ScheduleCellParams;
struct CameraSettingsDialogState;
struct AnalyticsEngineInfo;

class CameraSettingsDialogStore: public QObject
{
    Q_OBJECT
    using base_type = QObject;

public:
    explicit CameraSettingsDialogStore(QObject* parent = nullptr);
    virtual ~CameraSettingsDialogStore() override;

    const CameraSettingsDialogState& state() const;

    // Actions.
    void setReadOnly(bool value);
    void setSettingsOptimizationEnabled(bool value);
    void setGlobalPermissions(GlobalPermissions value);
    void setSingleWearableState(const WearableState& value);
    void loadCameras(const QnVirtualCameraResourceList& cameras);
    void setSingleCameraUserName(const QString& text);
    void setScheduleBrush(const ScheduleCellParams& brush);
    void setScheduleBrushRecordingType(Qn::RecordingType value);
    void setScheduleBrushFps(int value);
    void setScheduleBrushQuality(Qn::StreamQuality value);
    void setSchedule(const QnScheduleTaskList& schedule);
    void setRecordingShowFps(bool value);
    void setRecordingShowQuality(bool value);
    void toggleCustomBitrateVisible();
    void setCustomRecordingBitrateMbps(float mbps);
    void setCustomRecordingBitrateNormalized(float value);
    void setMinRecordingDaysAutomatic(bool value);
    void setMinRecordingDaysValue(int value);
    void setMaxRecordingDaysAutomatic(bool value);
    void setMaxRecordingDaysValue(int value);
    void setRecordingBeforeThresholdSec(int value);
    void setRecordingAfterThresholdSec(int value);
    void setCustomAspectRatio(const QnAspectRatio& value);
    void setCustomRotation(const Rotation& value);
    void setRecordingEnabled(bool value);
    void setAudioEnabled(bool value);
    void setMotionDetectionEnabled(bool value);
    void setMotionRegionList(const QList<QnMotionRegion>& value);
    void setFisheyeSettings(const QnMediaDewarpingParams& value);
    void setIoPortDataList(const QnIOPortDataList& value);
    void setIoModuleVisualStyle(vms::api::IoModuleVisualStyle value);
    void setCameraControlDisabled(bool value);
    void setDualStreamingDisabled(bool value);
    void setUseBitratePerGOP(bool value);
    void setPrimaryRecordingDisabled(bool value);
    void setSecondaryRecordingDisabled(bool value);
    void setPreferredPtzPresetType(nx::core::ptz::PresetType value);
    void setForcedPtzPanTiltCapability(bool value);
    void setForcedPtzZoomCapability(bool value);
    void setRtpTransportType(vms::api::RtpTransportType value);
    void setCustomMediaPortUsed(bool value);
    void setTrustCameraTime(bool value);
    void setCustomMediaPort(int value);
    void setMotionStreamType(vms::api::MotionStreamType value);
    void setLogicalId(int value);
    void generateLogicalId();
    void resetExpertSettings();

    Q_INVOKABLE QVariantList analyticsEngines() const;
    void setAnalyticsEngines(const QList<AnalyticsEngineInfo>& value);
    Q_INVOKABLE QVariantList enabledAnalyticsEngines() const;
    void setEnabledAnalyticsEngines(const QSet<QnUuid>& value);
    Q_INVOKABLE QnUuid currentAnalyticsEngineId() const;
    Q_INVOKABLE void setCurrentAnalyticsEngineId(const QnUuid& engineId);
    Q_INVOKABLE bool analyticsSettingsLoading() const;
    void setAnalyticsSettingsLoading(bool value);
    Q_INVOKABLE void setEnabledAnalyticsEngines(const QVariantList& value);
    Q_INVOKABLE QVariantMap deviceAgentSettingsValues(const QnUuid& engineId) const;
    Q_INVOKABLE void setDeviceAgentSettingsValues(
        const QnUuid& engineId, const QVariantMap& values);
    void resetDeviceAgentSettingsValues(
        const QnUuid& engineId, const QVariantMap& values);
    Q_INVOKABLE bool recordingEnabled() const;

    void setWearableMotionDetectionEnabled(bool value);
    void setWearableMotionSensitivity(int value);
    void setCredentials(const std::optional<QString>& login, const std::optional<QString>& password);
    void setStreamUrls(const QString& primary, const QString& secondary);

signals:
    void stateChanged(const CameraSettingsDialogState& state);

private:
    struct Private;
    QScopedPointer<Private> d;
};

} // namespace nx::vms::client::desktop
