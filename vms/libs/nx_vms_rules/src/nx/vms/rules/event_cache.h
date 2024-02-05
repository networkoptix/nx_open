// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <unordered_map>

#include <nx/utils/elapsed_timer.h>

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

private:
    std::unordered_map<QString, nx::utils::ElapsedTimer> m_cachedEvents;
    nx::utils::ElapsedTimer m_cacheTimer;
};

} // namespace nx::vms::rules
