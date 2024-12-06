// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "event.h"

#include <nx/analytics/taxonomy/abstract_state.h>
#include <nx/vms/api/data/user_settings.h>
#include <nx/vms/common/system_context.h>
#include <nx/vms/event/migration_utils.h>

#include "../engine.h"
#include "../event_filter.h"
#include "../event_filter_fields/analytics_event_type_field.h"
#include "../manifest.h"
#include "common.h"
#include "field.h"
#include "resource.h"

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

QSet<State> getPossibleFilterStates(EventDurationType eventDurationType)
{
    switch (eventDurationType)
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

bool hasDeviceField(const vms::rules::ItemDescriptor& eventDescriptor)
{
    return utils::hasResourceField(eventDescriptor, rules::ResourceType::device);
}

std::string getDeviceField(const vms::rules::ItemDescriptor& eventDescriptor)
{
    return utils::resourceField(eventDescriptor, rules::ResourceType::device);
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

EventDurationType getEventDurationType(const vms::rules::ItemDescriptor& eventDescriptor)
{
    const auto isInstant = eventDescriptor.flags.testFlag(ItemFlag::instant);
    const auto isProlonged = eventDescriptor.flags.testFlag(ItemFlag::prolonged);

    if (isInstant && isProlonged)
        return EventDurationType::mixed;

    if (isInstant)
        return EventDurationType::instant;

    if (isProlonged)
        return EventDurationType::prolonged;

    NX_ASSERT(false, "Event duration is not provided");
    return {};
}

EventDurationType getEventDurationType(
    const Engine* engine, const EventFilter* eventFilter)
{
    const auto descriptor = engine->eventDescriptor(eventFilter->eventType());
    if (!NX_ASSERT(descriptor))
        return {};

    auto result = getEventDurationType(descriptor.value());

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

QSet<State> getPossibleFilterStatesForEventDescriptor(
    const vms::rules::ItemDescriptor& eventDescriptor)
{
    return getPossibleFilterStates(getEventDurationType(eventDescriptor));
}

QSet<State> getPossibleFilterStatesForEventFilter(
    const Engine* engine, const EventFilter* eventFilter)
{
    return getPossibleFilterStates(getEventDurationType(engine, eventFilter));
}

bool isEventWatched(const nx::vms::api::UserSettings& settings, nx::vms::api::EventType eventType)
{
    return isEventWatched(settings, event::convertToNewEvent(eventType));
}

bool isEventWatched(const nx::vms::api::UserSettings& settings, const QString& eventType)
{
    return !settings.eventFilter.contains(eventType.toStdString());
}

} // namespace nx::vms::rules
