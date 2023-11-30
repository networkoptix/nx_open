// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <unordered_map>
#include <unordered_set>

#include <nx/utils/elapsed_timer.h>

#include "rules_fwd.h"

namespace nx::vms::rules {

/** Fast simple cache to discard frequent duplicate events. */
class NX_VMS_RULES_API EventCache
{
public:
    bool eventWasCached(const QString& cacheKey) const;
    void cacheEvent(const QString& cacheKey);

    void cleanupOldEventsFromCache(
        std::chrono::milliseconds eventTimeout,
        std::chrono::milliseconds cleanupTimeout);

    /** Check prolonged event on-off consistency.*/
    bool checkEventState(const EventPtr& event);

    std::chrono::microseconds runningEventStartTimestamp(const EventPtr& event) const;
    void stopRunningEvent(const EventPtr& event);

private:
    std::unordered_map<QString, nx::utils::ElapsedTimer> m_cachedEvents;
    nx::utils::ElapsedTimer m_cacheTimer;

    std::unordered_map<QString, std::chrono::microseconds> m_runningEvents;
};

} // namespace nx::vms::rules
