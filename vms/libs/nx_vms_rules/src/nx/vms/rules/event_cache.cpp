// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "event_cache.h"

#include <nx/utils/log/log.h>

#include "basic_event.h"
#include "utils/event.h"

namespace nx::vms::rules {

namespace {

using namespace std::chrono;

// The following timeouts got from the event rule processor.
constexpr auto kEventTimeout = 30s;
constexpr auto kCleanupTimeout = 5s;

} // namespace

void EventCache::cleanupOldEventsFromCache(
    std::chrono::milliseconds eventTimeout,
    std::chrono::milliseconds cleanupTimeout)
{
    if (!m_cacheTimer.hasExpired(cleanupTimeout))
        return;
    m_cacheTimer.restart();

    std::erase_if(
        m_cachedEvents,
        [eventTimeout](const auto& pair) { return pair.second.hasExpired(eventTimeout); });
}

bool EventCache::eventWasCached(const QString& cacheKey) const
{
    // TODO: #amalov Check for cached event expiration here.
    return !cacheKey.isEmpty() && m_cachedEvents.contains(cacheKey);
}

void EventCache::cacheEvent(const QString& eventKey)
{
    using namespace nx::utils;

    if (eventKey.isEmpty())
        return;

    cleanupOldEventsFromCache(kEventTimeout, kCleanupTimeout);
    m_cachedEvents.emplace(eventKey, ElapsedTimer(ElapsedTimerState::started));
}

std::chrono::microseconds EventCache::runningEventStartTimestamp(const EventPtr& event) const
{
    if (!event)
        std::chrono::microseconds::zero();

    const auto it = m_runningEvents.find(event->resourceKey());
    return it != m_runningEvents.end() ? it->second : std::chrono::microseconds::zero();
}

void EventCache::stopRunningEvent(const EventPtr& event)
{
    if (event && event->state() == State::stopped)
        m_runningEvents.erase(event->resourceKey());
}

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

        m_runningEvents.insert({resourceKey, event->timestamp()});
    }
    else
    {
        if (!isEventRunning)
        {
            NX_VERBOSE(this, "Event %1-%2 doesn't match due to stop without start",
                event->type(), resourceKey);
            return false;
        }
    }

    return true;
}

} // namespace nx::vms::rules
