// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "common.h"

#include <core/resource/media_server_resource.h>

#include "../action_builder.h"
#include "../engine.h"
#include "../event_filter.h"
#include "../event_filter_fields/customizable_flag_field.h"
#include "../rule.h"
#include "../utils/field.h"
#include "action.h"
#include "event.h"

namespace nx::vms::rules::utils {

std::optional<FieldDescriptor> fieldByName(
    const QString& fieldName, const ItemDescriptor& descriptor)
{
    for (const auto& field: descriptor.fields)
    {
        if (field.fieldName == fieldName)
            return field;
    }

    return std::nullopt;
}

bool isLoggingAllowed(const Engine* engine, nx::Uuid ruleId)
{
    const auto rule = engine->rule(ruleId);
    if (!rule) // It may be removed already.
        return false;

    const auto eventFilters = rule->eventFilters();
    if (!NX_ASSERT(!eventFilters.empty()))
        return false;

    // TODO: #mmalofeev this will not work properly when and/or logic will be implemented.
    // Consider move 'omit logging' property to the rule.
    const auto omitLoggingField =
        eventFilters.front()->fieldByName<CustomizableFlagField>(utils::kOmitLoggingFieldName);
    if (omitLoggingField && omitLoggingField->value())
        return false;

    const auto eventDescriptor = engine->eventDescriptor(eventFilters.front()->eventType());
    if (!NX_ASSERT(eventDescriptor))
        return false;

    const auto actionBuilders = rule->actionBuilders();
    if (!NX_ASSERT(!actionBuilders.empty()))
        return false;

    const auto actionDescriptor = engine->actionDescriptor(actionBuilders.front()->actionType());
    if (!NX_ASSERT(actionDescriptor))
        return false;

    return true;
}

bool isCompatible(
    const Engine* engine,
    const EventFilter* eventFilter,
    const ActionBuilder* actionBuilder)
{
    const auto availableStates =
        getAvailableStates(engine, eventFilter) & getAvailableStates(engine, actionBuilder);

    return !availableStates.empty();
}

QList<State> getAvailableStates(const Engine* engine, const Rule* rule)
{
    auto result = (getAvailableStates(engine, rule->eventFilters().first())
        & getAvailableStates(engine, rule->actionBuilders().first())).values();

    std::sort(result.begin(), result.end());

    return result;
}

} // namespace nx::vms::rules::utils
