// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <thread>

#include <gtest/gtest.h>

#include <nx/vms/event/event_cache.h>

namespace nx::vms::rules::test {

using namespace nx::vms::event;

TEST(EventCacheTest, cacheKey)
{
    EventCache cache;

    // Empty cache key events are not cached.
    EXPECT_FALSE(cache.isReportedBefore(""));
    cache.reportEvent("");
    EXPECT_FALSE(cache.isReportedBefore(""));

    // Same cache events are ignored.
    EXPECT_FALSE(cache.isReportedBefore("a"));
    cache.reportEvent("a");
    EXPECT_TRUE(cache.isReportedBefore("a"));

    // Different cache events are matched.
    EXPECT_FALSE(cache.isReportedBefore("b"));
    cache.reportEvent("b");
    EXPECT_TRUE(cache.isReportedBefore("b"));
}

TEST(EventCacheTest, cacheTimeout)
{
    using namespace std::chrono;

    EventCache cache;

    // Same cache events are ignored.
    cache.reportEvent("a");
    EXPECT_TRUE(cache.isReportedBefore("a"));

    // Same cache events are matched after timeout.
    std::this_thread::sleep_for(5ms);
    cache.cleanupOldEventsFromCache(1ms);
    EXPECT_FALSE(cache.isReportedBefore("a"));
}

} // nx::vms::rules::test
