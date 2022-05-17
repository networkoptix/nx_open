// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "state_field.h"

namespace nx::vms::rules {

bool StateField::match(const QVariant& eventValue) const
{
    // Accept any event if filter value is not set.
    if (value() == State::none)
        return true;

    return value() == eventValue.value<State>();
}

} // namespace nx::vms::rules
