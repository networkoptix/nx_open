// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "../basic_event.h"

namespace nx::vms::rules {

inline bool isProlonged(const EventPtr& event)
{
    const auto eventState = event->state();
    return eventState == State::started || eventState == State::stopped;
}

inline bool isInstant(const EventPtr& event)
{
    return event->state() == State::instant;
}

NX_VMS_RULES_API bool hasSourceCamera(const vms::rules::ItemDescriptor& eventDescriptor);

NX_VMS_RULES_API bool hasSourceServer(const vms::rules::ItemDescriptor& eventDescriptor);

/** Returns whether logging is allowed for the given event. */
NX_VMS_RULES_API bool isLoggingAllowed(const Engine* engine, const EventPtr& event);

} // namespace nx::vms::rules
