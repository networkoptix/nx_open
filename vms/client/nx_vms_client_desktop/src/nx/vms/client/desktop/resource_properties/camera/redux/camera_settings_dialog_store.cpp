#include "camera_settings_dialog_store.h"

#include <type_traits>

#include <QtCore/QScopedValueRollback>

#include <nx/vms/client/desktop/common/redux/private_redux_store.h>

#include "camera_settings_dialog_state.h"
#include "camera_settings_dialog_state_reducer.h"

namespace nx::vms::client::desktop {

using State = CameraSettingsDialogState;
using Reducer = CameraSettingsDialogStateReducer;

namespace {

template<typename T>
QVariantList toVariantList(const T& collection)
{
    QVariantList result;
    for (const auto& item: collection)
        result.append(QVariant::fromValue(item));
    return result;
}

template<typename L>
L fromVariantList(const QVariantList& list)
{
    using T = std::decay_t<decltype(*L().begin())>;

    L result;
    for (const auto& item: list)
        result.push_back(item.value<T>());
    return result;
}

} // namespace

struct CameraSettingsDialogStore::Private:
    PrivateReduxStore<CameraSettingsDialogStore, State>
{
    using PrivateReduxStore::PrivateReduxStore;
};

CameraSettingsDialogStore::CameraSettingsDialogStore(QObject* parent):
    base_type(parent),
    d(new Private(this))
{
}

CameraSettingsDialogStore::~CameraSettingsDialogStore() = default;

const State& CameraSettingsDialogStore::state() const
{
    return d->state;
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

void CameraSettingsDialogStore::setGlobalPermissions(GlobalPermissions value)
{
    d->executeAction(
        [&](State state) { return Reducer::setGlobalPermissions(std::move(state), value); });
}

void CameraSettingsDialogStore::setSingleWearableState(const WearableState& value)
{
    d->executeAction(
        [&](State state) { return Reducer::setSingleWearableState(std::move(state), value); });
}

void CameraSettingsDialogStore::loadCameras(const QnVirtualCameraResourceList& cameras)
{
    d->executeAction([&](State state) { return Reducer::loadCameras(std::move(state), cameras); });
}

void CameraSettingsDialogStore::setSingleCameraUserName(const QString& text)
{
    d->executeAction(
        [&](State state) { return Reducer::setSingleCameraUserName(std::move(state), text); });
}

void CameraSettingsDialogStore::setScheduleBrush(const ScheduleCellParams& brush)
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

void CameraSettingsDialogStore::setMinRecordingDaysAutomatic(bool value)
{
    d->executeAction(
        [&](State state)
        {
            return Reducer::setMinRecordingDaysAutomatic(std::move(state), value);
        });
}

void CameraSettingsDialogStore::setMinRecordingDaysValue(int value)
{
    d->executeAction(
        [&](State state) { return Reducer::setMinRecordingDaysValue(std::move(state), value); });
}

void CameraSettingsDialogStore::setMaxRecordingDaysAutomatic(bool value)
{
    d->executeAction(
        [&](State state)
        {
            return Reducer::setMaxRecordingDaysAutomatic(std::move(state), value);
        });
}

void CameraSettingsDialogStore::setMaxRecordingDaysValue(int value)
{
    d->executeAction(
        [&](State state) { return Reducer::setMaxRecordingDaysValue(std::move(state), value); });
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

void CameraSettingsDialogStore::setCustomRotation(const Rotation& value)
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

void CameraSettingsDialogStore::setMotionDetectionEnabled(bool value)
{
    d->executeAction(
        [&](State state) { return Reducer::setMotionDetectionEnabled(std::move(state), value); });
}

void CameraSettingsDialogStore::setMotionRegionList(const QList<QnMotionRegion>& value)
{
    d->executeAction(
        [&](State state) { return Reducer::setMotionRegionList(std::move(state), value); });
}

void CameraSettingsDialogStore::setFisheyeSettings(const QnMediaDewarpingParams& value)
{
    d->executeAction(
        [&](State state) { return Reducer::setFisheyeSettings(std::move(state), value); });
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

void CameraSettingsDialogStore::setDualStreamingDisabled(bool value)
{
    d->executeAction(
        [&](State state) { return Reducer::setDualStreamingDisabled(std::move(state), value); });
}

void CameraSettingsDialogStore::setUseBitratePerGOP(bool value)
{
    d->executeAction(
        [&](State state) { return Reducer::setUseBitratePerGOP(std::move(state), value); });
}

void CameraSettingsDialogStore::setPrimaryRecordingDisabled(bool value)
{
    d->executeAction(
        [&](State state) { return Reducer::setPrimaryRecordingDisabled(std::move(state), value); });
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

void CameraSettingsDialogStore::setRtpTransportType(nx::vms::api::RtpTransportType value)
{
    d->executeAction(
        [&](State state) { return Reducer::setRtpTransportType(std::move(state), value); });
}

void CameraSettingsDialogStore::setCustomMediaPortUsed(bool value)
{
    d->executeAction(
        [&](State state) { return Reducer::setCustomMediaPortUsed(std::move(state), value); });
}

void CameraSettingsDialogStore::setCustomMediaPort(int value)
{
    d->executeAction(
        [&](State state) { return Reducer::setCustomMediaPort(std::move(state), value); });
}

void CameraSettingsDialogStore::setTrustCameraTime(bool value)
{
    d->executeAction(
        [&](State state) { return Reducer::setTrustCameraTime(std::move(state), value); });
}

void CameraSettingsDialogStore::setForcedMotionStreamType(nx::vms::api::StreamIndex value)
{
    d->executeAction(
        [&](State state) { return Reducer::setForcedMotionStreamType(std::move(state), value); });
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

QVariantList CameraSettingsDialogStore::analyticsEngines() const
{
    QVariantList result;
    for (const auto& plugin: d->state.analytics.engines)
        result.append(plugin.toVariantMap());
    return result;
}

void CameraSettingsDialogStore::setAnalyticsEngines(const QList<AnalyticsEngineInfo>& value)
{
    d->executeAction(
        [&](State state) { return Reducer::setAnalyticsEngines(std::move(state), value); });
}

QVariantList CameraSettingsDialogStore::enabledAnalyticsEngines() const
{
    return toVariantList(d->state.analytics.enabledEngines.get());
}

void CameraSettingsDialogStore::setEnabledAnalyticsEngines(const QSet<QnUuid>& value)
{
    d->executeAction(
        [&](State state) { return Reducer::setEnabledAnalyticsEngines(std::move(state), value); });
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
    return d->state.analytics.loading;
}

void CameraSettingsDialogStore::setAnalyticsSettingsLoading(bool value)
{
    d->executeAction(
        [&](State state)
        {
            return Reducer::setAnalyticsSettingsLoading(std::move(state), value);
        });
}

void CameraSettingsDialogStore::setEnabledAnalyticsEngines(const QVariantList& value)
{
    setEnabledAnalyticsEngines(fromVariantList<QList<QnUuid>>(value).toSet());
}

QVariantMap CameraSettingsDialogStore::deviceAgentSettingsValues(const QnUuid& engineId) const
{
    return d->state.analytics.settingsValuesByEngineId.value(engineId).get();
}

void CameraSettingsDialogStore::setDeviceAgentSettingsValues(
    const QnUuid& engineId, const QVariantMap& values)
{
    d->executeAction(
        [&](State state)
        {
            return Reducer::setDeviceAgentSettingsValues(std::move(state), engineId, values);
        });
}

void CameraSettingsDialogStore::resetDeviceAgentSettingsValues(
    const QnUuid& engineId, const QVariantMap& values)
{
    d->executeAction(
        [&](State state)
        {
            return Reducer::resetDeviceAgentSettingsValues(std::move(state), engineId, values);
        });
}

bool CameraSettingsDialogStore::recordingEnabled() const
{
    return d->state.recording.enabled();
}

void CameraSettingsDialogStore::setWearableMotionDetectionEnabled(bool value)
{
    d->executeAction(
        [&](State state)
        {
            return Reducer::setWearableMotionDetectionEnabled(std::move(state), value);
        });
}

void CameraSettingsDialogStore::setWearableMotionSensitivity(int value)
{
    d->executeAction(
        [&](State state) { return Reducer::setWearableMotionSensitivity(std::move(state), value); });
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
        [&](State state) { return Reducer::setStreamUrls(std::move(state), primary, secondary); });
}

} // namespace nx::vms::client::desktop
