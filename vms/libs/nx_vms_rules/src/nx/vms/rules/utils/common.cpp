// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "common.h"

#include "../action_builder.h"
#include "../engine.h"
#include "../event_filter.h"
#include "../event_filter_fields/customizable_flag_field.h"
#include "../rule.h"
#include "../utils/field.h"

namespace nx::vms::rules::utils {

bool isLoggingAllowed(const Engine* engine, QnUuid ruleId)
{
    const auto rule = engine->rule(ruleId);
    if (!NX_ASSERT(rule))
        return false;

    const auto eventFilters = rule->eventFilters();
    if (!NX_ASSERT(!eventFilters.empty()))
        return false;

    // TODO: #mmlaofeev this will not work properly when and/or logic will be implemented.
    // Consider move 'omit logging' property to the rule.
    const auto omitLoggingField =
        eventFilters.front()->fieldByName<CustomizableFlagField>(utils::kOmitLoggingFieldName);
    if (omitLoggingField && omitLoggingField->value())
        return false;

    const auto eventDescriptor = engine->eventDescriptor(eventFilters.front()->eventType());
    if (!NX_ASSERT(eventDescriptor))
        return false;

    if (eventDescriptor->flags.testFlag(ItemFlag::omitLogging))
        return false;

    const auto actionBuilders = rule->actionBuilders();
    if (!NX_ASSERT(!actionBuilders.empty()))
        return false;

    const auto actionDescriptor = engine->actionDescriptor(actionBuilders.front()->actionType());
    if (!NX_ASSERT(actionDescriptor))
        return false;

    if (actionDescriptor->flags.testFlag(ItemFlag::omitLogging))
        return false;

    return true;
}

} // namespace nx::vms::rules::utils
