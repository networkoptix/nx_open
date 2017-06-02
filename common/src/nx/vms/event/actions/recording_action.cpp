#include "recording_action.h"

#include <nx/vms/event/action_parameters.h>

namespace nx {
namespace vms {
namespace event {

RecordingAction::RecordingAction(const EventParameters& runtimeParams):
    base_type(CameraRecordingAction, runtimeParams)
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

int RecordingAction::getRecordDuration() const
{
    return m_params.recordingDuration;
}

int RecordingAction::getRecordAfter() const
{
    return m_params.recordAfter;
}

} // namespace event
} // namespace vms
} // namespace nx
