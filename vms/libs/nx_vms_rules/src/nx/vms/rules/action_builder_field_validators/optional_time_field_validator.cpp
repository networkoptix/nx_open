// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "optional_time_field_validator.h"

#include <nx/utils/log/to_string.h>

#include "../action_builder_fields/optional_time_field.h"
#include "../strings.h"

namespace nx::vms::rules {

ValidationResult OptionalTimeFieldValidator::validity(
    const Field* field, const Rule* /*rule*/, common::SystemContext* /*context*/) const
{
    auto optionalTimeField = dynamic_cast<const OptionalTimeField*>(field);
    if (!NX_ASSERT(optionalTimeField))
        return {QValidator::State::Invalid, Strings::invalidFieldType()};

    const auto timeFieldValue = optionalTimeField->value();
    if (optionalTimeField->value() == std::chrono::microseconds::zero())
        return {};

    const auto timeFieldProperties = optionalTimeField->properties();
    if (timeFieldValue < timeFieldProperties.minimumValue)
    {
        return {
            .validity = QValidator::State::Invalid,
            .description = tr("Value can not be less than %1").arg(
                toString(timeFieldProperties.minimumValue))};
    }

    if (timeFieldValue > timeFieldProperties.maximumValue)
    {
        return {
            .validity = QValidator::State::Invalid,
            .description = tr("Value can not be more than %1").arg(
                toString(timeFieldProperties.maximumValue))};
    }

    return {};
}

} // namespace nx::vms::rules
