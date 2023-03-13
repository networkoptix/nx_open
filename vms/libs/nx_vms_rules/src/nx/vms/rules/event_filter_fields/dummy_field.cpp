// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "dummy_field.h"

namespace nx::vms::rules {

bool DummyField::match(const QVariant& /*value*/) const
{
    return true;
}

} // namespace nx::vms::rules
