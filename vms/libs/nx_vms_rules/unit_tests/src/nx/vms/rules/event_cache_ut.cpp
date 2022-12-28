// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <thread>

#include <nx/vms/rules/event_cache.h>

namespace nx::vms::rules::test {

TEST(EventCacheTest, cacheKey)
{
    EventCache cache;

    // Empty cache key events are not cached.
    EXPECT_FALSE(cache.eventWasCached(""));
    cache.cacheEvent("");
    EXPECT_FALSE(cache.eventWasCached(""));

    // Same cache events are ignored.
    EXPECT_FALSE(cache.eventWasCached("a"));
    cache.cacheEvent("a");
    EXPECT_TRUE(cache.eventWasCached("a"));

    // Different cache events are matched.
    EXPECT_FALSE(cache.eventWasCached("b"));
    cache.cacheEvent("b");
    EXPECT_TRUE(cache.eventWasCached("b"));
}

TEST(EventCacheTest, cacheTimeout)
{
    using namespace std::chrono;

    EventCache cache;

    // Same cache events are ignored.
    cache.cacheEvent("a");
    EXPECT_TRUE(cache.eventWasCached("a"));

    // Same cache events are matched after timeout.
    std::this_thread::sleep_for(5ms);
    cache.cleanupOldEventsFromCache(1ms, 1ms);
    EXPECT_FALSE(cache.eventWasCached("a"));
}

} // nx::vms::rules::test
