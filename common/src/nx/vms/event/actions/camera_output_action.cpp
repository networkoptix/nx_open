#include "camera_output_action.h"

#include <nx/vms/event/action_parameters.h>

namespace {
static const int kInstantActionAutoResetTimeoutMs = 30000;
} // namespace

namespace nx {
namespace vms {
namespace event {

CameraOutputAction::CameraOutputAction(const EventParameters& runtimeParams):
    base_type(cameraOutputAction, runtimeParams)
{
//     if (instant)
//         m_params.relayAutoResetTimeout = kInstantActionAutoResetTimeoutMs; // default value for instant action
}

QString CameraOutputAction::getRelayOutputId() const
{
    return m_params.relayOutputId;
}

int CameraOutputAction::getRelayAutoResetTimeout() const
{
    return m_params.durationMs;
}

QString CameraOutputAction::getExternalUniqKey() const
{
    return base_type::getExternalUniqKey() + QString(L'_') + getRelayOutputId();
}

} // namespace event
} // namespace vms
} // namespace nx
