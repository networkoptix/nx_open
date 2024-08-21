// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "event.h"

#include <nx/analytics/taxonomy/abstract_state.h>
#include <nx/vms/common/system_context.h>

#include "../engine.h"
#include "../event_filter.h"
#include "../event_filter_fields/analytics_event_type_field.h"
#include "../manifest.h"
#include "common.h"
#include "field.h"

namespace nx::vms::rules {

namespace {

EventDurationType eventGroupDuration(
    const std::shared_ptr<analytics::taxonomy::AbstractState>& taxonomyState,
    const QString& groupId)
{
    const auto eventTypes = taxonomyState->eventTypes();
    if (eventTypes.empty())
        return EventDurationType::instant;

    for (const auto& eventType: eventTypes)
    {
        for (const auto& scope: eventType->scopes())
        {
            const auto group = scope->group();
            if (group && (group->id() == groupId))
            {
                if (!eventType->isStateDependent())
                    return EventDurationType::instant;
            }
        }
    }

    return EventDurationType::prolonged;
}

} // namespace

bool hasSourceCamera(const vms::rules::ItemDescriptor& eventDescriptor)
{
    return eventDescriptor.resources.contains(utils::kCameraIdFieldName)
        || eventDescriptor.resources.contains(utils::kDeviceIdsFieldName);
}

bool hasSourceServer(const vms::rules::ItemDescriptor& eventDescriptor)
{
    return std::any_of(
        eventDescriptor.fields.begin(),
        eventDescriptor.fields.end(),
        [](const vms::rules::FieldDescriptor& fieldDescriptor)
        {
            return fieldDescriptor.fieldName == vms::rules::utils::kServerIdFieldName;
        });
}

bool hasSourceUser(const vms::rules::ItemDescriptor& eventDescriptor)
{
    return utils::fieldByName(utils::kUserIdFieldName, eventDescriptor).has_value();
}

nx::Uuid sourceId(const BasicEvent* event)
{
    const auto getId =
        [event](const char* propName)
        {
            return event->property(propName).value<nx::Uuid>();
        };

    if (const auto cameraId = getId(utils::kCameraIdFieldName); !cameraId.isNull())
        return cameraId;

    if (const auto serverId = getId(utils::kServerIdFieldName); !serverId.isNull())
        return serverId;

    return {};
}

EventDurationType getEventDurationType(
    const Engine* engine, const EventFilter* eventFilter)
{
    const auto descriptor = engine->eventDescriptor(eventFilter->eventType());
    if (!NX_ASSERT(descriptor))
        return {};

    const auto isInstant = descriptor->flags.testFlag(ItemFlag::instant);
    const auto isProlonged = descriptor->flags.testFlag(ItemFlag::prolonged);

    EventDurationType result{};
    if (isInstant && isProlonged)
    {
        result = EventDurationType::mixed;
    }
    else if (isInstant)
    {
        result = EventDurationType::instant;
    }
    else if (isProlonged)
    {
        result = EventDurationType::prolonged;
    }
    else
    {
        NX_ASSERT(false, "Event duration is not provided");
        return {};
    }

    if (const auto analyticsEventTypeField =
        eventFilter->fieldByName<AnalyticsEventTypeField>(utils::kEventTypeIdFieldName))
    {
        if (analyticsEventTypeField->typeId().isNull())
            return result;

        const auto taxonomyState = engine->systemContext()->analyticsTaxonomyState();
        if (!NX_ASSERT(taxonomyState))
            return result;

        const auto eventType = taxonomyState->eventTypeById(analyticsEventTypeField->typeId());
        if (eventType)
        {
            result = eventType->isStateDependent()
                ? EventDurationType::prolonged
                : EventDurationType::instant;
        }
        else
        {
            result = eventGroupDuration(taxonomyState, analyticsEventTypeField->typeId());
        }
    }

    return result;
}

QSet<State> getAvailableStates(
    const Engine* engine, const EventFilter* eventFilter)
{
    switch (getEventDurationType(engine, eventFilter))
    {
        case EventDurationType::instant:
            return {State::instant};
        case EventDurationType::prolonged:
            return {State::none, State::started, State::stopped};
        case EventDurationType::mixed:
            return {
                State::none,
                State::started,
                State::stopped,
                State::instant};
        default:
            NX_ASSERT(false, "Unexpected event duration");
            return {};
    }
}

} // namespace nx::vms::rules
