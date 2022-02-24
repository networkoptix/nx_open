// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "recording_action.h"

#include <nx/vms/event/action_parameters.h>

namespace nx {
namespace vms {
namespace event {

RecordingAction::RecordingAction(const EventParameters& runtimeParams):
    base_type(ActionType::cameraRecordingAction, runtimeParams)
{
}

int RecordingAction::getFps() const
{
    return m_params.fps;
}

Qn::StreamQuality RecordingAction::getStreamQuality() const
{
    return m_params.streamQuality;
}

int RecordingAction::getDurationSec() const
{
    return m_params.durationMs / 1000;
}

int RecordingAction::getRecordAfterSec() const
{
    return m_params.recordAfter;
}

int RecordingAction::getRecordBeforeSec() const
{
    return m_params.recordBeforeMs / 1000;
}

} // namespace event
} // namespace vms
} // namespace nx
