// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "text_field.h"

namespace nx::vms::rules {

bool EventTextField::match(const QVariant& value) const
{
    if (this->value().isEmpty())
        return true;

    return SimpleTypeEventField::match(value);
}

} // namespace nx::vms::rules
