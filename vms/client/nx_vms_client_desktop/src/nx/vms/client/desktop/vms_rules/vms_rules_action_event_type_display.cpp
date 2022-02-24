// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "vms_rules_action_event_type_display.h"

#include <nx/utils/log/assert.h>

#include <nx/vms/rules/event_filter.h>
#include <nx/vms/rules/action_builder.h>

namespace nx::vms::client::desktop {
namespace vms_rules {

QString getDisplayName(const nx::vms::rules::EventFilter* event)
{
    if (event->eventType() == "GenericEvent")
        return "Generic event"; //< TODO: Make strings translatable when it will be appropriate.

    //NX_ASSERT(false, "Unexpected event type");
    return event->eventType();
}

QString getDisplayName(const nx::vms::rules::ActionBuilder* action)
{
    if (action->actionType() == "nx.http")
        return "Do HTTP request"; //< TODO: Make strings translatable when it will be appropriate.

    //NX_ASSERT(false, "Unexpected action type");
    return action->actionType();
}

} // namespace vms_rules
} // namespace nx::vms::client::desktop
