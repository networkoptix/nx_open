// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "customizable_text_field.h"

#include <QtCore/QVariant>

#include <nx/utils/log/assert.h>

namespace nx::vms::rules {

bool CustomizableTextField::match(const QVariant& /*eventValue*/) const
{
    // Field value used for event customization, matches any event value.
    return true;
}

} // namespace nx::vms::rules
