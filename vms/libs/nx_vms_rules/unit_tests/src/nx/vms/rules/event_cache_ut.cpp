// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <thread>

#include <gtest/gtest.h>

#include <nx/vms/api/rules/rule.h>
#include <nx/vms/event/event_cache.h>
#include <nx/vms/rules/action_builder.h>
#include <nx/vms/rules/event_filter.h>
#include <nx/vms/rules/event_filter_fields/source_camera_field.h>
#include <nx/vms/rules/rule.h>
#include <nx/vms/rules/utils/api.h>

#include "test_plugin.h"
#include "test_router.h"

namespace nx::vms::rules::test {

using namespace std::chrono;

class EventCacheTest: public EngineBasedTest, public TestPlugin
{
public:
    EventCacheTest(): TestPlugin(engine.get()) {};

    nx::vms::event::EventCache* cache() const { return engine->eventCache(); }
};

TEST_F(EventCacheTest, eventReporting)
{
    auto& cache = *this->cache();

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

TEST_F(EventCacheTest, cacheCleanup)
{
    auto& cache = *this->cache();

    // Same cache events are ignored.
    cache.reportEvent("a");
    EXPECT_TRUE(cache.isReportedBefore("a"));

    // Same cache events are matched after timeout.
    std::this_thread::sleep_for(5ms);
    cache.cleanupOldEventsFromCache(1ms);
    EXPECT_FALSE(cache.isReportedBefore("a"));
}

TEST_F(EventCacheTest, cacheKey)
{
    auto rule = makeRule<TestEventInstant, TestAction>();
    engine->updateRule(serialize(rule.get()));

    auto event = TestEventInstantPtr::create(std::chrono::microseconds::zero(), State::instant);

    // Empty cache key events are not cached.
    EXPECT_EQ(engine->processEvent(event), 1);
    EXPECT_EQ(engine->processEvent(event), 1);

    // Same cache events are ignored.
    event->setCacheKey("a");
    EXPECT_EQ(engine->processEvent(event), 1);
    EXPECT_EQ(engine->processEvent(event), 0);

    // Different cache events are matched.
    event->setCacheKey("b");
    EXPECT_EQ(engine->processEvent(event), 1);
    EXPECT_EQ(engine->processEvent(event), 0);
}

TEST_F(EventCacheTest, cacheTimeout)
{
    auto rule = makeRule<TestEventInstant, TestAction>();
    engine->updateRule(serialize(rule.get()));

    auto event = TestEventInstantPtr::create(std::chrono::microseconds::zero(), State::instant);

    // Same cache events are ignored.
    event->setCacheKey("a");
    EXPECT_EQ(engine->processEvent(event), 1);
    EXPECT_EQ(engine->processEvent(event), 0);

    // Same cache events are matched after timeout.
    std::this_thread::sleep_for(5ms);
    engine->eventCache()->cleanupOldEventsFromCache(1ms);
    EXPECT_EQ(engine->processEvent(event), 1);
    EXPECT_EQ(engine->processEvent(event), 0);
}

TEST_F(EventCacheTest, cachedEventReprocessedIfNotReported)
{
    const auto cameraA = nx::Uuid::createUuid();
    const auto cameraB = nx::Uuid::createUuid();

    auto rule = makeRule<TestEventInstant, TestAction>();
    auto filter = rule->eventFilters().front();
    auto cameraField = filter->fieldByType<SourceCameraField>();

    cameraField->setAcceptAll(false);
    cameraField->setIds({cameraA});
    engine->updateRule(serialize(rule.get()));

    auto event = TestEventInstantPtr::create(std::chrono::microseconds::zero(), State::instant);
    event->m_cameraId = cameraB;
    event->setCacheKey("a");

    EXPECT_EQ(engine->processEvent(event), 0);
    event->m_cameraId = cameraA;

    // Event stops reprocessing if reported recently.
    EXPECT_EQ(engine->processEvent(event), 1);
    EXPECT_EQ(engine->processEvent(event), 0);
}

} // nx::vms::rules::test
