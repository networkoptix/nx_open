// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "state_field.h"

namespace nx::vms::rules {

bool StateField::match(const QVariant& eventValue) const
{
    const auto state = eventValue.value<State>();

    // Filter value State::none is used for actions with a duration equal to the event duration.
    if (value() == State::none)
        return state == State::started || state == State::stopped;

    return value() == state;
}

} // namespace nx::vms::rules
