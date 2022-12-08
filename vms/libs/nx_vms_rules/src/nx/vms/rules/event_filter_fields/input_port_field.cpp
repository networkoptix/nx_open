// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "input_port_field.h"

namespace nx::vms::rules {

bool InputPortField::match(const QVariant& eventValue) const
{
    if (value().isEmpty())
        return true;

    return SimpleTypeEventField::match(eventValue);
}

} // namespace nx::vms::rules
