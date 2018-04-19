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
    static State setPanicMode(State state, bool value);
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
    static State setMotionDetectionEnabled(State state, bool value);
    static State setMotionRegionList(State state, const QList<QnMotionRegion>& value);
};

} // namespace desktop
} // namespace client
} // namespace nx