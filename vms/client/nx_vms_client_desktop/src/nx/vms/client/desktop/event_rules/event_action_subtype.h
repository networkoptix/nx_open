#pragma once

#include <nx/vms/api/types/event_rule_types.h>

namespace nx::vms::client::desktop {

enum class EventSubtype
{
    user,
    failure,
    success,
    undefined,
};

enum class ActionSubtype
{
    server,
    client,
    undefined,
};

EventSubtype eventSubtype(nx::vms::api::EventType eventType);

ActionSubtype actionSubtype(nx::vms::api::ActionType actionType);

QList<nx::vms::api::EventType> filterEventsBySubtype(
    const QList<nx::vms::api::EventType>& events,
    EventSubtype eventSubtype);

QList<nx::vms::api::ActionType> filterActionsBySubtype(
    const QList<nx::vms::api::ActionType>& actions,
    ActionSubtype actionSubtype);

} // namespace nx::vms::client::desktop
