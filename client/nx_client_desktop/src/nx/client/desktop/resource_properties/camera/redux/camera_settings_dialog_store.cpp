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

    void executeAction(std::function<State(State)> reduce)
    {
        // Chained actions are forbidden.
        if (actionInProgress)
            return;

        QScopedValueRollback<bool> guard(actionInProgress, true);
        state = reduce(state);
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
    d->executeAction([](State state) { return Reducer::applyChanges(state); });
}

void CameraSettingsDialogStore::loadCameras(const QnVirtualCameraResourceList& cameras)
{
    d->executeAction([&](State state) { return Reducer::loadCameras(state, cameras); });
}

void CameraSettingsDialogStore::setSingleCameraUserName(const QString& text)
{
    d->executeAction([&](State state) { return Reducer::setSingleCameraUserName(state, text); });
}

void CameraSettingsDialogStore::setScheduleBrushRecordingType(Qn::RecordingType value)
{
    d->executeAction([&](State state) { return Reducer::setScheduleBrushRecordingType(state, value); });
}

void CameraSettingsDialogStore::setScheduleBrushFps(int value)
{
    d->executeAction([&](State state) { return Reducer::setScheduleBrushFps(state, value); });
}

void CameraSettingsDialogStore::setScheduleBrushQuality(Qn::StreamQuality value)
{
    d->executeAction([&](State state) { return Reducer::setScheduleBrushQuality(state, value); });
}

void CameraSettingsDialogStore::setRecordingShowFps(bool value)
{
    d->executeAction([&](State state) { return Reducer::setRecordingShowFps(state, value); });
}

void CameraSettingsDialogStore::setRecordingShowQuality(bool value)
{
    d->executeAction([&](State state) { return Reducer::setRecordingShowQuality(state, value); });
}

void CameraSettingsDialogStore::toggleCustomBitrateVisible()
{
    d->executeAction([&](State state) { return Reducer::toggleCustomBitrateVisible(state); });
}

void CameraSettingsDialogStore::setCustomRecordingBitrateMbps(float mbps)
{
    d->executeAction(
        [&](State state) { return Reducer::setCustomRecordingBitrateMbps(state, mbps); });
}

void CameraSettingsDialogStore::setCustomRecordingBitrateNormalized(float value)
{
    d->executeAction(
        [&](State state) { return Reducer::setCustomRecordingBitrateNormalized(state, value); });
}

void CameraSettingsDialogStore::setMinRecordingDaysAutomatic(bool value)
{
    d->executeAction(
        [&](State state) { return Reducer::setMinRecordingDaysAutomatic(state, value); });
}

void CameraSettingsDialogStore::setMinRecordingDaysValue(int value)
{
    d->executeAction(
        [&](State state) { return Reducer::setMinRecordingDaysValue(state, value); });
}

void CameraSettingsDialogStore::setMaxRecordingDaysAutomatic(bool value)
{
    d->executeAction(
        [&](State state) { return Reducer::setMaxRecordingDaysAutomatic(state, value); });
}

void CameraSettingsDialogStore::setMaxRecordingDaysValue(int value)
{
    d->executeAction(
        [&](State state) { return Reducer::setMaxRecordingDaysValue(state, value); });
}

} // namespace desktop
} // namespace client
} // namespace nx
