// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "input_port_field.h"

namespace nx::vms::rules {

bool InputPortField::match(const QVariant& eventValue) const
{
    if (value().isEmpty())
        return true;

    return SimpleTypeEventField::match(eventValue);
}

QJsonObject InputPortField::openApiDescriptor(const QVariantMap& properties)
{
    auto descriptor = SimpleTypeField::openApiDescriptor(properties);
    descriptor[utils::kDescriptionProperty] = "the I/O Module port to take signal from. "
        "Empty value matches any input port.";
    return descriptor;
}

} // namespace nx::vms::rules
