// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "customizable_flag_field.h"

namespace nx::vms::rules {

bool CustomizableFlagField::match(const QVariant& /*value*/) const
{
    // Field value used for event customization, matches any event value.
    return true;
}

} // namespace nx::vms::rules
