#include "camera_settings_dialog_store.h"

#include <QtCore/QScopedValueRollback>

#include "camera_settings_dialog_state.h"
#include "camera_settings_dialog_state_reducer.h"

namespace nx {
namespace client {
namespace desktop {

using State = CameraSettingsDialogState;
using Reducer = CameraSettingsDialogStateReducer;

struct CameraSettingsDialogStore::Private
{
    CameraSettingsDialogStore* const q;
    bool actionInProgress = false;
    State state;

    explicit Private(CameraSettingsDialogStore* owner):
        q(owner)
    {
    }

    void executeAction(std::function<State(State&&)> reduce)
    {
        // Chained actions are forbidden.
        if (actionInProgress)
            return;

        QScopedValueRollback<bool> guard(actionInProgress, true);
        state = reduce(std::move(state));
        emit q->stateChanged(state);
    }
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

void CameraSettingsDialogStore::applyChanges()
{
    d->executeAction([](State state) { return Reducer::applyChanges(std::move(state)); });
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

void CameraSettingsDialogStore::setGlobalPermissions(Qn::GlobalPermissions value)
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

void CameraSettingsDialogStore::setCustomRecordingBitrateMbps(float mbps)
{
    d->executeAction(
        [&](State state)
        {
            return Reducer::setCustomRecordingBitrateMbps(std::move(state), mbps);
        });
}

void CameraSettingsDialogStore::setCustomRecordingBitrateNormalized(float value)
{
    d->executeAction(
        [&](State state)
        {
            return Reducer::setCustomRecordingBitrateNormalized(std::move(state), value);
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

void CameraSettingsDialogStore::setIoModuleVisualStyle(vms::api::IoModuleVisualStyle value)
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

void CameraSettingsDialogStore::setNativePtzPresetsDisabled(bool value)
{
    d->executeAction(
        [&](State state) { return Reducer::setNativePtzPresetsDisabled(std::move(state), value); });
}

void CameraSettingsDialogStore::setRtpTransportType(vms::api::RtpTransportType value)
{
    d->executeAction(
        [&](State state) { return Reducer::setRtpTransportType(std::move(state), value); });
}

void CameraSettingsDialogStore::setMotionStreamType(vms::api::MotionStreamType value)
{
    d->executeAction(
        [&](State state) { return Reducer::setMotionStreamType(std::move(state), value); });
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

} // namespace desktop
} // namespace client
} // namespace nx
