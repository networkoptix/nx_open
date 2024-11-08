// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "camera_output_action.h"

#include <nx/vms/event/action_parameters.h>

namespace nx::vms::event {

CameraOutputAction::CameraOutputAction(const EventParameters& runtimeParams):
    base_type(ActionType::cameraOutputAction, runtimeParams)
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
    return base_type::getExternalUniqKey() + '_' + getRelayOutputId();
}

} // namespace nx::vms::event
