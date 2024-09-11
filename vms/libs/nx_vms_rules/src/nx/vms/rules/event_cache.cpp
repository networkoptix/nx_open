// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "event_cache.h"

#include "basic_event.h"
#include "utils/event.h"

#include <nx/utils/log/log.h>

namespace nx::vms::rules {


bool EventCache::checkEventState(const EventPtr& event)
{
    if (!isProlonged(event))
        return true;

    // It is required to check if the prolonged events are coming in the right order.
    // Each stopped event must follow started event. Repeated start and stop events
    // with the same key should be ignored.

    const auto resourceKey = event->resourceKey();
    const bool isEventRunning = m_runningEvents.contains(resourceKey);

    if (event->state() == State::started)
    {
        if (isEventRunning)
        {
            NX_VERBOSE(this, "Event %1-%2 doesn't match due to repeated start",
                event->type(), resourceKey);
            return false;
        }

        m_runningEvents.insert(resourceKey);
    }
    else
    {
        if (!isEventRunning)
        {
            NX_VERBOSE(this, "Event %1-%2 doesn't match due to stop without start",
                event->type(), resourceKey);
            return false;
        }

        m_runningEvents.erase(resourceKey);
    }

    return true;
}

} // namespace nx::vms::rules
