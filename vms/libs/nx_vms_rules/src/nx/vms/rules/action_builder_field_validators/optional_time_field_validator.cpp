// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "optional_time_field_validator.h"

#include <nx/utils/log/to_string.h>

#include "../action_builder_fields/optional_time_field.h"
#include "../event_filter.h"
#include "../event_filter_fields/state_field.h"
#include "../rule.h"
#include "../strings.h"
#include "../utils/field.h"

namespace nx::vms::rules {

ValidationResult OptionalTimeFieldValidator::validity(
    const Field* field, const Rule* rule, common::SystemContext* /*context*/) const
{
    auto optionalTimeField = dynamic_cast<const OptionalTimeField*>(field);
    if (!NX_ASSERT(optionalTimeField))
        return {QValidator::State::Invalid, Strings::invalidFieldType()};

    const auto timeFieldValue = optionalTimeField->value();
    if (timeFieldValue < std::chrono::microseconds::zero())
    {
        return {
            .validity = QValidator::State::Invalid,
            .description = tr("Duration cannot be less than zero")
        };
    }

    const auto stateField =
        rule->eventFilters().first()->fieldByName<StateField>(utils::kStateFieldName);
    const auto isDurationField =
        optionalTimeField->descriptor()->fieldName == utils::kDurationFieldName;

    if (timeFieldValue == std::chrono::microseconds::zero())
    {
        if (isDurationField
            && stateField
            && stateField->value() != State::none)
        {
            return {
                .validity = QValidator::State::Invalid,
                .description = tr("Zero duration cannot be set for the `%1` event state")
                    .arg(toString(stateField->value()))
            };
        }

        return {};
    }

    if (isDurationField && stateField && stateField->value() == State::none)
    {
        return {
            .validity = QValidator::State::Invalid,
            .description = tr("Non zero duration cannot be set for the `%1` event state")
                .arg(toString(stateField->value()))
        };
    }

    const auto timeFieldProperties = optionalTimeField->properties();
    if (timeFieldValue < timeFieldProperties.minimumValue)
    {
        return {
            .validity = QValidator::State::Invalid,
            .description = tr("Value cannot be less than %1").arg(
                toString(timeFieldProperties.minimumValue))};
    }

    if (timeFieldValue > timeFieldProperties.maximumValue)
    {
        return {
            .validity = QValidator::State::Invalid,
            .description = tr("Value cannot be more than %1").arg(
                toString(timeFieldProperties.maximumValue))};
    }

    return {};
}

} // namespace nx::vms::rules
