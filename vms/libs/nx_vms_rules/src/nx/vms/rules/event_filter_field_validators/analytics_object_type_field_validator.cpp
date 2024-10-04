// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "analytics_object_type_field_validator.h"

#include "../event_filter_fields/analytics_object_type_field.h"
#include "../strings.h"

namespace nx::vms::rules {

ValidationResult AnalyticsObjectTypeFieldValidator::validity(
    const Field* field, const Rule* /*rule*/, common::SystemContext* /*context*/) const
{
    auto analyticsObjectTypeField = dynamic_cast<const AnalyticsObjectTypeField*>(field);
    if (!NX_ASSERT(analyticsObjectTypeField))
        return {QValidator::State::Invalid, {Strings::invalidFieldType()}};

    const auto objectType = analyticsObjectTypeField->value();
    if (objectType.isEmpty())
        return {QValidator::State::Invalid, "Analytics object type is not selected"};

    // TODO: #mmalofeev check if objectType is a valid type for the selected devices.

    return {};
}

} // namespace nx::vms::rules
