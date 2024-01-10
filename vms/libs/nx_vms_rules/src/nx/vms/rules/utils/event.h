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

/**
 * Returns event source to display in notification tile and tooltip.
 * Keep in sync with AbstractEvent.getResource().
 */
NX_VMS_RULES_API QnUuid sourceId(const BasicEvent* event);

NX_VMS_RULES_API bool hasSourceCamera(const vms::rules::ItemDescriptor& eventDescriptor);

NX_VMS_RULES_API bool hasSourceServer(const vms::rules::ItemDescriptor& eventDescriptor);

} // namespace nx::vms::rules
