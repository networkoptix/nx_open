// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "common.h"

#include <core/resource/media_server_resource.h>

#include "../action_builder.h"
#include "../engine.h"
#include "../event_filter.h"
#include "../event_filter_fields/customizable_flag_field.h"
#include "../event_filter_fields/state_field.h"
#include "../rule.h"
#include "../utils/field.h"
#include "action.h"

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
    if (!NX_ASSERT(rule))
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

bool isCompatible(const ItemDescriptor& eventDescriptor, const ItemDescriptor& actionDescriptor)
{
    if (utils::isInstantOnly(eventDescriptor) && utils::isProlongedOnly(actionDescriptor))
    {
        // Instant event might be used with the prolonged action only if the action has 'duration'.
        return std::any_of(
            actionDescriptor.fields.cbegin(),
            actionDescriptor.fields.cend(),
            [](const auto& fieldDescriptor)
            {
                return fieldDescriptor.fieldName == vms::rules::utils::kDurationFieldName;
            });
    }

    return true;
}

bool isCompatible(
    const Engine* engine, const StateField* stateField, const EventFilter* eventFilter)
{
    const auto eventDescription = engine->eventDescriptor(eventFilter->eventType());

    return stateField->value() == State::instant
        ? eventDescription->flags.testFlag(ItemFlag::instant)
        : eventDescription->flags.testFlag(ItemFlag::prolonged);
}

bool isCompatible(
    const Engine* engine, const StateField* stateField, const ActionBuilder* actionBuilder)
{
    const auto isActionProlonged = isProlonged(engine, actionBuilder);

    return stateField->value() == State::none
        ? isActionProlonged
        : !isActionProlonged;
}

bool isCompatible(
    const Engine* engine, const EventFilter* eventFilter, const ActionBuilder* actionBuilder)
{
    const auto eventDescriptor = engine->eventDescriptor(eventFilter->eventType());
    const auto actionDescriptor = engine->actionDescriptor(actionBuilder->actionType());
    if (!eventDescriptor || !actionDescriptor)
        return false;

    if (!isCompatible(eventDescriptor.value(), actionDescriptor.value()))
        return false;

    if (const auto stateField = eventFilter->fieldByName<StateField>(kStateFieldName))
    {
        if (!isCompatible(engine, stateField, eventFilter)
            || !isCompatible(engine, stateField, actionBuilder))
        {
            return false;
        }
    }

    return true;
}

} // namespace nx::vms::rules::utils
