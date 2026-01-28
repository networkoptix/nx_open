// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "state_field.h"

namespace nx::vms::rules {

bool StateField::match(const QVariant& eventValue) const
{
    const auto eventState = eventValue.value<State>();

    // Filter value State::none is used for actions with a duration equal to the event duration.
    if (value() == State::none)
        return eventState == State::started || eventState == State::stopped;
    // Filter value State::instant accept both instant & started events for backwards compatibility
    // and to support Analytics event groups with mixed duration.
    else if (value() == State::instant)
        return eventState == State::instant || eventState == State::started;

    return value() == eventState;
}

} // namespace nx::vms::rules
