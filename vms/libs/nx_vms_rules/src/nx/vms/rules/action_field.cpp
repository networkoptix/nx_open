// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "action_field.h"

namespace nx::vms::rules {

ActionField::ActionField()
{
}

QSet<QString> ActionField::requiredEventFields() const
{
    return {};
}

} // namespace nx::vms::rules
