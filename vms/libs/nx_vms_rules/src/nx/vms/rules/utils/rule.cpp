// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "rule.h"

#include "../action_builder_field.h"
#include "../event_filter_field.h"
#include "../manifest.h"

namespace nx::vms::rules::utils {

ValidationResult visibleFieldsValidity(const Rule* rule, common::SystemContext* context)
{
    static const auto filter =
        []<typename Field>(const Field* field)
        {
            return field->descriptor()->properties.value("visible", true).toBool();
        };

    return rule->validity(context, filter, filter); //< Check only visible fields.
}

} // namespace nx::vms::rules::utils
