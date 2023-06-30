// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "camera_settings_dialog_store.h"

#include <type_traits>

#include <QtCore/QScopedValueRollback>

#include <nx/reflect/json/serializer.h>
#include <nx/utils/guarded_callback.h>
#include <nx/utils/log/log.h>
#include <nx/utils/qt_helpers.h>
#include <nx/vms/client/desktop/common/flux/private_flux_store.h>
#include <nx/vms/time/formatter.h>

#include "camera_settings_dialog_state_reducer.h"

namespace nx::vms::client::desktop {

using State = CameraSettingsDialogState;
using Reducer = CameraSettingsDialogStateReducer;

using namespace nx::vms::api;

struct CameraSettingsDialogStore::Private:
    PrivateFluxStore<CameraSettingsDialogStore, State>
{
    using PrivateFluxStore::PrivateFluxStore;
};

CameraSettingsDialogStore::CameraSettingsDialogStore(QObject* parent):
    base_type(parent),
    d(new Private(this))
{
    connect(this, &CameraSettingsDialogStore::stateChanged,
        this,
        [this]
        {
            NX_VERBOSE(this, "State changed to: %1", nx::reflect::json::serialize(d->state));
        });

    // Qt6 workaround for QML crash when passing a reference to move-only in signal parameter.
    connect(this, &CameraSettingsDialogStore::stateChanged,
        this, &CameraSettingsDialogStore::stateModified);
}

CameraSettingsDialogStore::~CameraSettingsDialogStore() = default;

const State& CameraSettingsDialogStore::state() const
{
    return d->state;
}

void CameraSettingsDialogStore::loadCameras(
    const QnVirtualCameraResourceList& cameras,
    DeviceAgentSettingsAdapter* deviceAgentSettingsAdapter,
    CameraSettingsAnalyticsEnginesWatcherInterface* analyticsEnginesWatcher,
    CameraAdvancedParametersManifestManager* advancedParametersManifestManager)
{
    d->executeAction(
        [&](State state)
        {
            return Reducer::loadCameras(
                std::move(state),
                cameras,
                deviceAgentSettingsAdapter,
                analyticsEnginesWatcher,
                advancedParametersManifestManager);
        });
}

void CameraSettingsDialogStore::handleMotionTypeChanged(const QnVirtualCameraResourceList& cameras)
{
    d->executeAction(
        [&](State state) { return Reducer::handleMotionTypeChanged(std::move(state), cameras); });
}

void CameraSettingsDialogStore::handleMotionForcedChanged(
    const QnVirtualCameraResourceList& cameras)
{
    d->executeAction(
        [&](State state)
        {
            return Reducer::handleMotionForcedChanged(std::move(state), cameras);
        });
}

void CameraSettingsDialogStore::handleDualStreamingChanged(
    const QnVirtualCameraResourceList& cameras)
{
    d->executeAction(
        [&](State state)
        {
            return Reducer::handleDualStreamingChanged(std::move(state), cameras);
        });
}

void CameraSettingsDialogStore::handleMediaStreamsChanged(
    const QnVirtualCameraResourceList& cameras)
{
    d->executeAction(
        [&](State state)
        {
            return Reducer::handleMediaStreamsChanged(std::move(state), cameras);
        });
}

void CameraSettingsDialogStore::handleMotionStreamChanged(
    const QnVirtualCameraResourceList& cameras)
{
    d->executeAction(
        [&](State state)
        {
            return Reducer::handleMotionStreamChanged(std::move(state), cameras);
        });
}

void CameraSettingsDialogStore::handleMotionRegionsChanged(
    const QnVirtualCameraResourceList& cameras)
{
    d->executeAction(
        [&](State state)
        {
            return Reducer::handleMotionRegionsChanged(std::move(state), cameras);
        });
}

void CameraSettingsDialogStore::handleStreamUrlsChanged(
    const QnVirtualCameraResourceList& cameras)
{
    d->executeAction(
        [&](State state)
        {
            return Reducer::handleStreamUrlsChanged(std::move(state), cameras);
        });
}

void CameraSettingsDialogStore::handleCompatibleObjectTypesMaybeChanged(
    const QnVirtualCameraResourceList& cameras)
{
    d->executeAction(
        [&](State state)
        {
            return Reducer::handleCompatibleObjectTypesMaybeChanged(std::move(state), cameras);
        });
}

void CameraSettingsDialogStore::handleStatusChanged(const QnVirtualCameraResourceList& cameras)
{
    d->executeAction(
        [&](State state) { return Reducer::handleStatusChanged(std::move(state), cameras); });
}

void CameraSettingsDialogStore::handleMediaCapabilitiesChanged(
    const QnVirtualCameraResourceList& cameras)
{
    d->executeAction(
        [&](State state)
        {
            return Reducer::handleMediaCapabilitiesChanged(std::move(state), cameras);
        });
}

void CameraSettingsDialogStore::handleAudioEnabledChanged(
    const QnVirtualCameraResourceList& cameras)
{
    d->executeAction(
        [&](State state)
        {
            return Reducer::handleAudioEnabledChanged(std::move(state), cameras);
        });
}

void CameraSettingsDialogStore::updatePtzSettings(const QnVirtualCameraResourceList& cameras)
{
    d->executeAction(
        [&](State state) { return Reducer::updatePtzSettings(std::move(state), cameras); });
}

void CameraSettingsDialogStore::setSelectedTab(CameraSettingsTab value)
{
    d->executeAction([&](State state) { return Reducer::setSelectedTab(std::move(state), value); });
}

void CameraSettingsDialogStore::setReadOnly(bool value)
{
    d->executeAction([&](State state) { return Reducer::setReadOnly(std::move(state), value); });
}

void CameraSettingsDialogStore::setSettingsOptimizationEnabled(bool value)
{
    d->executeAction(
        [&](State state)
        {
            return Reducer::setSettingsOptimizationEnabled(std::move(state), value);
        });
}

void CameraSettingsDialogStore::setHasPowerUserPermissions(bool value)
{
    d->executeAction(
        [&](State state) { return Reducer::setHasPowerUserPermissions(std::move(state), value); });
}

void CameraSettingsDialogStore::setHasEventLogPermission(bool value)
{
    d->executeAction(
        [&](State state) { return Reducer::setHasEventLogPermission(std::move(state), value); });
}

void CameraSettingsDialogStore::setHasEditAccessRightsForAllCameras(bool value)
{
    d->executeAction(
        [&](State state)
        {
            return Reducer::setHasEditAccessRightsForAllCameras(std::move(state), value);
        });
}

void CameraSettingsDialogStore::setPermissions(Qn::Permissions value)
{
    d->executeAction(
        [&](State state)
        {
            return Reducer::setPermissions(std::move(state), value);
        });
}

void CameraSettingsDialogStore::setAllCamerasEditable(bool value)
{
    d->executeAction(
        [&](State state)
        {
            return Reducer::setAllCamerasEditable(std::move(state), value);
        });
}

void CameraSettingsDialogStore::setSaasInitialized(bool value)
{
    d->executeAction(
        [&](State state) { return Reducer::setSaasInitialized(std::move(state), value); });
}

void CameraSettingsDialogStore::setSingleVirtualCameraState(const VirtualCameraState& value)
{
    d->executeAction(
        [&](State state) { return Reducer::setSingleVirtualCameraState(std::move(state), value); });
}

void CameraSettingsDialogStore::setSingleCameraUserName(const QString& text)
{
    d->executeAction(
        [&](State state) { return Reducer::setSingleCameraUserName(std::move(state), text); });
}

void CameraSettingsDialogStore::setSingleCameraIsOnline(bool isOnline)
{
    d->executeAction(
        [&](State state) { return Reducer::setSingleCameraIsOnline(std::move(state), isOnline); });
}

void CameraSettingsDialogStore::setScheduleBrush(const RecordScheduleCellData& brush)
{
    d->executeAction(
        [&](State state) { return Reducer::setScheduleBrush(std::move(state), brush); });
}

void CameraSettingsDialogStore::setScheduleBrushRecordingType(Qn::RecordingType value)
{
    d->executeAction(
        [&](State state)
        {
            return Reducer::setScheduleBrushRecordingType(std::move(state), value);
        });
}

void CameraSettingsDialogStore::setRecordingMetadataRadioButton(
    CameraSettingsDialogState::MetadataRadioButton value)
{
    d->executeAction(
        [&](State state)
        {
            return Reducer::setRecordingMetadataRadioButton(std::move(state), value);
        });
}

void CameraSettingsDialogStore::setScheduleBrushFps(int value)
{
    d->executeAction(
        [&](State state) { return Reducer::setScheduleBrushFps(std::move(state), value); });
}

void CameraSettingsDialogStore::setScheduleBrushQuality(Qn::StreamQuality value)
{
    d->executeAction(
        [&](State state) { return Reducer::setScheduleBrushQuality(std::move(state), value); });
}

void CameraSettingsDialogStore::setSchedule(const QnScheduleTaskList& schedule)
{
    d->executeAction(
        [&](State state) { return Reducer::setSchedule(std::move(state), schedule); });
}

void CameraSettingsDialogStore::fixupSchedule()
{
    d->executeAction([&](State state) { return Reducer::fixupSchedule(std::move(state)); });
}

void CameraSettingsDialogStore::setRecordingShowFps(bool value)
{
    d->executeAction(
        [&](State state) { return Reducer::setRecordingShowFps(std::move(state), value); });
}

void CameraSettingsDialogStore::setRecordingShowQuality(bool value)
{
    d->executeAction(
        [&](State state) { return Reducer::setRecordingShowQuality(std::move(state), value); });
}

void CameraSettingsDialogStore::toggleCustomBitrateVisible()
{
    d->executeAction(
        [&](State state) { return Reducer::toggleCustomBitrateVisible(std::move(state)); });
}

void CameraSettingsDialogStore::setRecordingBitrateMbps(float mbps)
{
    d->executeAction(
        [&](State state)
        {
            return Reducer::setRecordingBitrateMbps(std::move(state), mbps);
        });
}

void CameraSettingsDialogStore::setRecordingBitrateNormalized(float value)
{
    d->executeAction(
        [&](State state)
        {
            return Reducer::setRecordingBitrateNormalized(std::move(state), value);
        });
}

void CameraSettingsDialogStore::setMinRecordingPeriodAutomatic(bool value)
{
    d->executeAction(
        [&](State state)
        {
            return Reducer::setMinRecordingPeriodAutomatic(std::move(state), value);
        });
}

void CameraSettingsDialogStore::setMinRecordingPeriodValue(std::chrono::seconds value)
{
    d->executeAction(
        [&](State state) { return Reducer::setMinRecordingPeriodValue(std::move(state), value); });
}

void CameraSettingsDialogStore::setMaxRecordingPeriodAutomatic(bool value)
{
    d->executeAction(
        [&](State state)
        {
            return Reducer::setMaxRecordingPeriodAutomatic(std::move(state), value);
        });
}

void CameraSettingsDialogStore::setMaxRecordingPeriodValue(std::chrono::seconds value)
{
    d->executeAction(
        [&](State state) { return Reducer::setMaxRecordingPeriodValue(std::move(state), value); });
}

void CameraSettingsDialogStore::setRecordingBeforeThresholdSec(int value)
{
    d->executeAction(
        [&](State state)
        {
            return Reducer::setRecordingBeforeThresholdSec(std::move(state), value);
        });
}

void CameraSettingsDialogStore::setRecordingAfterThresholdSec(int value)
{
    d->executeAction(
        [&](State state)
        {
            return Reducer::setRecordingAfterThresholdSec(std::move(state), value);
        });
}

void CameraSettingsDialogStore::setCustomAspectRatio(const QnAspectRatio& value)
{
    d->executeAction(
        [&](State state) { return Reducer::setCustomAspectRatio(std::move(state), value); });
}

void CameraSettingsDialogStore::setCustomRotation(core::StandardRotation value)
{
    d->executeAction(
        [&](State state) { return Reducer::setCustomRotation(std::move(state), value); });
}

void CameraSettingsDialogStore::setRecordingEnabled(bool value)
{
    d->executeAction(
        [&](State state) { return Reducer::setRecordingEnabled(std::move(state), value); });
}

void CameraSettingsDialogStore::setAudioEnabled(bool value)
{
    d->executeAction(
        [&](State state) { return Reducer::setAudioEnabled(std::move(state), value); });
}

void CameraSettingsDialogStore::setTwoWayAudioEnabled(bool value)
{
    d->executeAction(
        [&](State state) { return Reducer::setTwoWayAudioEnabled(std::move(state), value); });
}

void CameraSettingsDialogStore::setAudioInputDeviceId(const QnUuid& deviceId)
{
    d->executeAction(
        [&](State state) { return Reducer::setAudioInputDeviceId(std::move(state), deviceId); });
}

void CameraSettingsDialogStore::setAudioOutputDeviceId(const QnUuid& deviceId)
{
    d->executeAction(
        [&](State state) { return Reducer::setAudioOutputDeviceId(std::move(state), deviceId); });
}

void CameraSettingsDialogStore::setCameraHotspotsEnabled(bool value)
{
    d->executeAction(
        [&](State state) { return Reducer::setCameraHotspotsEnabled(std::move(state), value); });
}

void CameraSettingsDialogStore::setCameraHotspotsData(
    const nx::vms::common::CameraHotspotDataList & cameraHotspots)
{
    d->executeAction(
        [&](State state)
        {
            return Reducer::setCameraHotspotsData(std::move(state), cameraHotspots);
        });
}

void CameraSettingsDialogStore::setMotionDetectionEnabled(bool value)
{
    d->executeAction(
        [&](State state)
        {
            return Reducer::setMotionDetectionEnabled(std::move(state), value);
        });
}

void CameraSettingsDialogStore::forceMotionDetection()
{
    d->executeAction([&](State state) { return Reducer::forceMotionDetection(std::move(state)); });
}

void CameraSettingsDialogStore::setCurrentMotionSensitivity(int value)
{
    d->executeAction(
        [&](State state) { return Reducer::setCurrentMotionSensitivity(std::move(state), value); });
}

void CameraSettingsDialogStore::setMotionRegionList(const QList<QnMotionRegion>& value)
{
    d->executeAction(
        [&](State state) { return Reducer::setMotionRegionList(std::move(state), value); });
}

void CameraSettingsDialogStore::setMotionDependingOnDualStreaming(CombinedValue value)
{
    d->executeAction(
        [&](State state)
        {
            return Reducer::setMotionDependingOnDualStreaming(std::move(state), value);
        });
}

void CameraSettingsDialogStore::setDewarpingParams(const dewarping::MediaData& value)
{
    d->executeAction(
        [&](State state) { return Reducer::setDewarpingParams(std::move(state), value); });
}

nx::vms::client::core::MediaDewarpingParams CameraSettingsDialogStore::dewarpingParams() const
{
    return nx::vms::client::core::MediaDewarpingParams(
        d->state.singleCameraSettings.dewarpingParams());
}

void CameraSettingsDialogStore::setDewarpingParams(
    bool enabled,
    qreal centerX,
    qreal centerY,
    qreal radius,
    qreal stretch,
    qreal rollCorrectionDegrees,
    dewarping::FisheyeCameraMount cameraMount,
    dewarping::CameraProjection cameraProjection,
    qreal sphereAlpha,
    qreal sphereBeta)
{
    dewarping::MediaData data;
    data.enabled = enabled;
    data.xCenter = centerX;
    data.yCenter = centerY;
    data.radius = radius;
    data.hStretch = stretch;
    data.fovRot = rollCorrectionDegrees;
    data.viewMode = cameraMount;
    data.cameraProjection = cameraProjection;
    data.sphereAlpha = sphereAlpha;
    data.sphereBeta = sphereBeta;
    setDewarpingParams(data);
}

void CameraSettingsDialogStore::setIoPortDataList(const QnIOPortDataList& value)
{
    d->executeAction(
        [&](State state) { return Reducer::setIoPortDataList(std::move(state), value); });
}

void CameraSettingsDialogStore::setIoModuleVisualStyle(nx::vms::api::IoModuleVisualStyle value)
{
    d->executeAction(
        [&](State state) { return Reducer::setIoModuleVisualStyle(std::move(state), value); });
}

void CameraSettingsDialogStore::setCameraControlDisabled(bool value)
{
    d->executeAction(
        [&](State state) { return Reducer::setCameraControlDisabled(std::move(state), value); });
}

void CameraSettingsDialogStore::setKeepCameraTimeSettings(bool value)
{
    d->executeAction(
        [&](State state) { return Reducer::setKeepCameraTimeSettings(std::move(state), value); });
}

void CameraSettingsDialogStore::setDualStreamingDisabled(bool value)
{
    d->executeAction(
        [&](State state)
        {
            return Reducer::setDualStreamingDisabled(std::move(state), value);
        });
}

void CameraSettingsDialogStore::setUseBitratePerGOP(bool value)
{
    d->executeAction(
        [&](State state) { return Reducer::setUseBitratePerGOP(std::move(state), value); });
}

void CameraSettingsDialogStore::setUseMedia2ToFetchProfiles(nx::core::resource::UsingOnvifMedia2Type value)
{
    d->executeAction(
        [&](State state) { return Reducer::setUseMedia2ToFetchProfiles(std::move(state), value); });
}

void CameraSettingsDialogStore::setPrimaryRecordingDisabled(bool value)
{
    d->executeAction(
        [&](State state) { return Reducer::setPrimaryRecordingDisabled(std::move(state), value); });
}

void CameraSettingsDialogStore::setRecordAudioEnabled(bool value)
{
    d->executeAction(
        [&](State state) { return Reducer::setRecordAudioEnabled(std::move(state), value); });
}

void CameraSettingsDialogStore::setSecondaryRecordingDisabled(bool value)
{
    d->executeAction(
        [&](State state) { return Reducer::setSecondaryRecordingDisabled(std::move(state), value); });
}

void CameraSettingsDialogStore::setPreferredPtzPresetType(nx::core::ptz::PresetType value)
{
    d->executeAction(
        [&](State state) { return Reducer::setPreferredPtzPresetType(std::move(state), value); });
}

void CameraSettingsDialogStore::setForcedPtzPanTiltCapability(bool value)
{
    d->executeAction(
        [&](State state) { return Reducer::setForcedPtzPanTiltCapability(std::move(state), value); });
}

void CameraSettingsDialogStore::setForcedPtzZoomCapability(bool value)
{
    d->executeAction(
        [&](State state) { return Reducer::setForcedPtzZoomCapability(std::move(state), value); });
}

void CameraSettingsDialogStore::setDoNotSendStopPtzCommand(bool value)
{
    d->executeAction(
        [&](State state) { return Reducer::setDoNotSendStopPtzCommand(std::move(state), value); });
}

void CameraSettingsDialogStore::setDifferentPtzPanTiltSensitivities(bool value)
{
    d->executeAction(
        [&](State state) { return Reducer::setDifferentPtzPanTiltSensitivities(std::move(state), value); });
}

void CameraSettingsDialogStore::setRemoteArchiveMotionDetectionEnabled(bool value)
{
    d->executeAction(
        [&](State state)
        {
            return Reducer::setRemoteArchiveMotionDetectionEnabled(std::move(state), value);
        });
}

void CameraSettingsDialogStore::setPtzPanSensitivity(qreal value)
{
    d->executeAction(
        [&](State state) { return Reducer::setPtzPanSensitivity(std::move(state), value); });
}

void CameraSettingsDialogStore::setPtzTiltSensitivity(qreal value)
{
    d->executeAction(
        [&](State state) { return Reducer::setPtzTiltSensitivity(std::move(state), value); });
}

void CameraSettingsDialogStore::setRtpTransportType(nx::vms::api::RtpTransportType value)
{
    d->executeAction(
        [&](State state) { return Reducer::setRtpTransportType(std::move(state), value); });
}

void CameraSettingsDialogStore::setForcedPrimaryProfile(const QString& value)
{
    d->executeAction(
        [&](State state) { return Reducer::setForcedPrimaryProfile(std::move(state), value); });
}

void CameraSettingsDialogStore::setForcedSecondaryProfile(const QString& value)
{
    d->executeAction(
        [&](State state) { return Reducer::setForcedSecondaryProfile(std::move(state), value); });
}

void CameraSettingsDialogStore::setRemoteArchiveSyncronizationEnabled(bool value)
{
    d->executeAction(
        [&](State state)
        {
            return Reducer::setRemoteArchiveSyncronizationEnabled(std::move(state), value);
        });
}

void CameraSettingsDialogStore::setAutoMediaPortUsed(bool value)
{
    d->executeAction(
        [&](State state) { return Reducer::setCustomMediaPortUsed(std::move(state), !value); });
}

void CameraSettingsDialogStore::setCustomMediaPort(int value)
{
    d->executeAction(
        [&](State state) { return Reducer::setCustomMediaPort(std::move(state), value); });
}

void CameraSettingsDialogStore::setCustomWebPagePort(int value)
{
    d->executeAction(
        [&](State state) { return Reducer::setCustomWebPagePort(std::move(state), value); });
}

void CameraSettingsDialogStore::setTrustCameraTime(bool value)
{
    d->executeAction(
        [&](State state) { return Reducer::setTrustCameraTime(std::move(state), value); });
}

void CameraSettingsDialogStore::setMotionStream(nx::vms::api::StreamIndex value)
{
    d->executeAction(
        [&](State state) { return Reducer::setMotionStream(std::move(state), value); });
}

void CameraSettingsDialogStore::setLogicalId(int value)
{
    d->executeAction(
        [&](State state) { return Reducer::setLogicalId(std::move(state), value); });
}

void CameraSettingsDialogStore::generateLogicalId()
{
    d->executeAction(
        [&](State state) { return Reducer::generateLogicalId(std::move(state)); });
}

void CameraSettingsDialogStore::resetExpertSettings()
{
    d->executeAction(
        [&](State state) { return Reducer::resetExpertSettings(std::move(state)); });
}

QnUuid CameraSettingsDialogStore::resourceId() const
{
    return d->state.singleCameraId();
}

QVariantList CameraSettingsDialogStore::analyticsEngines() const
{
    QVariantList result;
    for (const auto& plugin: d->state.analytics.engines)
        result.append(plugin.toVariantMap());
    return result;
}

void CameraSettingsDialogStore::setAnalyticsEngines(const QList<core::AnalyticsEngineInfo>& value)
{
    d->executeAction(
        [&](State state) { return Reducer::setAnalyticsEngines(std::move(state), value); });
}

QVariantList CameraSettingsDialogStore::userEnabledAnalyticsEngines() const
{
    return nx::utils::toQVariantList(d->state.analytics.userEnabledEngines.get());
}

void CameraSettingsDialogStore::setUserEnabledAnalyticsEngines(const QSet<QnUuid>& value)
{
    d->executeAction(
        [&](State state)
        {
            return Reducer::setUserEnabledAnalyticsEngines(std::move(state), value);
        });
}

QnUuid CameraSettingsDialogStore::currentAnalyticsEngineId() const
{
    return d->state.analytics.currentEngineId;
}

void CameraSettingsDialogStore::setCurrentAnalyticsEngineId(const QnUuid& value)
{
    d->executeAction(
        [&](State state)
        {
            return Reducer::setCurrentAnalyticsEngineId(std::move(state), value);
        });
}

bool CameraSettingsDialogStore::analyticsSettingsLoading() const
{
    return d->state.analytics.settingsByEngineId.value(currentAnalyticsEngineId()).loading;
}

void CameraSettingsDialogStore::setUserEnabledAnalyticsEngines(const QVariantList& value)
{
    setUserEnabledAnalyticsEngines(nx::utils::toQSet(nx::utils::toTypedQList<QnUuid>(value)));
}

int CameraSettingsDialogStore::analyticsStreamIndex(const QnUuid& engineId) const
{
    return int(d->state.analytics.streamByEngineId.value(engineId)());
}

void CameraSettingsDialogStore::setAnalyticsStreamIndex(const QnUuid& engineId, int value)
{
    if (!NX_ASSERT(value <= 1))
        value = std::clamp(value, 0, 1);

    setAnalyticsStreamIndex(engineId, State::StreamIndex(value));
}

void CameraSettingsDialogStore::setAnalyticsStreamIndex(
    const QnUuid& engineId, nx::vms::api::StreamIndex value, ModificationSource source)
{
    d->executeAction(
        [&](State state)
        {
            return Reducer::setAnalyticsStreamIndex(
                std::move(state), engineId, State::StreamIndex(value), source);
        });
}

QJsonObject CameraSettingsDialogStore::deviceAgentSettingsModel(const QnUuid& engineId) const
{
    return d->state.analytics.settingsByEngineId.value(engineId).model;
}

QJsonObject CameraSettingsDialogStore::deviceAgentSettingsValues(const QnUuid& engineId) const
{
    return d->state.analytics.settingsByEngineId.value(engineId).values.get();
}

void CameraSettingsDialogStore::setDeviceAgentSettingsValues(
    const QnUuid& engineId,
    const QString& activeElement,
    const QJsonObject& values,
    const QJsonObject& paramValues)
{
    d->executeAction(
        [&](State state)
        {
            return Reducer::setDeviceAgentSettingsValues(
                std::move(state), engineId, activeElement, values, paramValues);
        });
}
void CameraSettingsDialogStore::handleOverusedEngines(const QSet<QnUuid>& overusedEngines)
{
    d->executeAction(
        [&](State state)
        {
            return Reducer::handleOverusedEngines(std::move(state), overusedEngines);
        });
}

void CameraSettingsDialogStore::refreshDeviceAgentSettings(const QnUuid& engineId)
{
    d->executeAction(
        [&](State state)
        {
            return Reducer::refreshDeviceAgentSettings(std::move(state), engineId);
        });
}

void CameraSettingsDialogStore::resetDeviceAgentData(
    const QnUuid& engineId, const core::DeviceAgentData& data, bool resetUser)
{
    d->executeAction(
        [&](State state)
        {
            return Reducer::resetDeviceAgentData(std::move(state), engineId, data, resetUser);
        });
}

QJsonObject CameraSettingsDialogStore::deviceAgentSettingsErrors(const QnUuid& engineId) const
{
    return d->state.analytics.settingsByEngineId[engineId].errors;
}

bool CameraSettingsDialogStore::dualStreamingEnabled() const
{
    return d->state.isDualStreamingEnabled();
}

bool CameraSettingsDialogStore::recordingEnabled() const
{
    return d->state.recording.enabled();
}

void CameraSettingsDialogStore::setVirtualCameraIgnoreTimeZone(bool value)
{
    d->executeAction(
        [&](State state)
        {
            return Reducer::setVirtualCameraIgnoreTimeZone(std::move(state), value);
        });
}

void CameraSettingsDialogStore::setVirtualCameraMotionDetectionEnabled(bool value)
{
    d->executeAction(
        [&](State state)
        {
            return Reducer::setVirtualCameraMotionDetectionEnabled(std::move(state), value);
        });
}

void CameraSettingsDialogStore::setVirtualCameraMotionSensitivity(int value)
{
    d->executeAction(
        [&](State state) { return Reducer::setVirtualCameraMotionSensitivity(std::move(state), value); });
}

void CameraSettingsDialogStore::setCredentials(
    const std::optional<QString>& login, const std::optional<QString>& password)
{
    if (!login && !password)
        return;

    d->executeAction(
        [&](State state) { return Reducer::setCredentials(std::move(state), login, password); });
}

void CameraSettingsDialogStore::setStreamUrls(const QString& primary, const QString& secondary)
{
    d->executeAction(
        [&](State state)
        {
            return Reducer::setStreamUrls(std::move(state), primary, secondary);
        });
}

void CameraSettingsDialogStore::setAdvancedSettingsManifest(const QnCameraAdvancedParams& manifest)
{
    d->executeAction(
        [&](State state)
        {
            return Reducer::setAdvancedSettingsManifest(std::move(state), manifest);
        });
}

void CameraSettingsDialogStore::setHasExportPermission(bool value)
{
    d->executeAction([&](State state)
        {
            return Reducer::setHasExportPermission(std::move(state), value);
        });
}

void CameraSettingsDialogStore::setScreenRecordingOn(bool value)
{
    d->executeAction([&](State state)
        {
            return Reducer::setScreenRecordingOn(std::move(state), value);
        });
}

bool CameraSettingsDialogStore::isReadOnly() const
{
    return d->state.readOnly;
}

} // namespace nx::vms::client::desktop
