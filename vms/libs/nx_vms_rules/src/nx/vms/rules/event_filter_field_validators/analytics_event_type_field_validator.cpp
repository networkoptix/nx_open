// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "analytics_event_type_field_validator.h"

#include "../event_filter_fields/analytics_event_type_field.h"
#include "../strings.h"

namespace nx::vms::rules {

ValidationResult AnalyticsEventTypeFieldValidator::validity(
    const Field* field, const Rule* /*rule*/, common::SystemContext* /*context*/) const
{
    auto analyticsEventTypeField = dynamic_cast<const AnalyticsEventTypeField*>(field);
    if (!NX_ASSERT(analyticsEventTypeField))
        return {QValidator::State::Invalid, {Strings::invalidFieldType()}};

    const auto typeId = analyticsEventTypeField->typeId();
    if (typeId.isEmpty())
        return {QValidator::State::Invalid, tr("Analytics event type is not selected")};

    // TODO: #mmalofeev check if typeId is a valid type for the selected devices.

    return {};
}

} // namespace nx::vms::rules
