// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "event_cache.h"

namespace nx::vms::rules {

namespace {

using namespace std::chrono;

// The following timeouts got from the event rule processor.
constexpr auto kEventTimeout = 3s;
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
    const auto it = m_cachedEvents.find(cacheKey);
    return it != m_cachedEvents.end()
        && !it->second.hasExpired(kEventTimeout);
}

void EventCache::cacheEvent(const QString& eventKey)
{
    using namespace nx::utils;

    if (eventKey.isEmpty())
        return;

    cleanupOldEventsFromCache(kEventTimeout, kCleanupTimeout);
    m_cachedEvents.emplace(eventKey, ElapsedTimer(ElapsedTimerState::started));
}

} // namespace nx::vms::rules
