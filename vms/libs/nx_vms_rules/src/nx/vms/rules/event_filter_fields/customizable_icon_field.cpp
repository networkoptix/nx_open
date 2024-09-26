// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "customizable_icon_field.h"

#include <QtCore/QVariant>

#include <nx/utils/log/assert.h>

namespace nx::vms::rules {

bool CustomizableIconField::match(const QVariant& value) const
{
    // Field value used for event customization, matches any event value.
    return true;
}

QJsonObject CustomizableIconField::openApiDescriptor(const QVariantMap& properties)
{
    auto descriptor = SimpleTypeEventField::openApiDescriptor(properties);
    descriptor[utils::kDescriptionProperty] =
        "Icon name, chosen from the system's predefined set.";
    descriptor[utils::kExampleProperty] = "_bell_on";
    return descriptor;
}

} // namespace nx::vms::rules
