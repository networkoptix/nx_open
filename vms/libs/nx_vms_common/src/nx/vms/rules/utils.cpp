// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "utils.h"

#include <nx/vms/api/types/event_rule_types.h>

namespace nx::vms::rules {

nx::vms::api::rules::State convertEventState(nx::vms::api::EventState eventState)
{
    using nx::vms::api::EventState;
    using nx::vms::api::rules::State;

    switch (eventState)
    {
        case EventState::inactive:
            return State::stopped;

        case EventState::active:
            return State::started;

        case EventState::undefined:
            return State::instant;

        default:
            return State::none;
    }
}

} // namespace nx::vms::api::rules
