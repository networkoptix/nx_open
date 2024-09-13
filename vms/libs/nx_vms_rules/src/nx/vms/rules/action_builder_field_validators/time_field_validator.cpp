// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "time_field_validator.h"

#include "../action_builder_fields/time_field.h"
#include "../strings.h"

namespace nx::vms::rules {

ValidationResult TimeFieldValidator::validity(
    const Field* field, const Rule* /*rule*/, common::SystemContext* /*context*/) const
{
    auto timeField = dynamic_cast<const TimeField*>(field);
    if (!NX_ASSERT(timeField))
        return {QValidator::State::Invalid, Strings::invalidFieldType()};

    const auto timeFieldValue = timeField->value();
    if (timeFieldValue < std::chrono::microseconds::zero())
    {
        return {
            .validity = QValidator::State::Invalid,
            .description = Strings::negativeTime()
        };
    }

    const auto timeFieldProperties = timeField->properties();
    if (timeFieldValue < timeFieldProperties.minimumValue
        || timeFieldValue > timeFieldProperties.maximumValue)
    {
        return {
            .validity = QValidator::State::Invalid,
            .description = Strings::invalidDuration(
                timeFieldValue, timeFieldProperties.minimumValue, timeFieldProperties.maximumValue)};
    }

    return {};
}

} // namespace nx::vms::rules
