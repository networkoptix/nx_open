// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <memory>

#include <gtest/gtest.h>

#include <nx/vms/rules/aggregator.h>
#include <nx/vms/rules/basic_event.h>
#include <nx/vms/time/abstract_time_sync_manager.h>
#include <utils/common/synctime.h>

#include "test_event.h"

using namespace std::chrono;
using namespace std::chrono_literals;

namespace nx::vms::rules::test {

namespace {

constexpr auto kDefaultAggregationInterval = 1ms;

Aggregator makeAggregator(milliseconds aggregationInterval = kDefaultAggregationInterval)
{
    return Aggregator(
        aggregationInterval,
        [](const EventPtr& event) { return event->objectName(); });
}

EventPtr makeEvent(const QString& name = QString())
{
    auto event = TestEventPtr::create(qnSyncTime->currentTimePoint());
    event->setObjectName(name);
    return event;
}

} // namespace

class TimeSyncManagerStub: public common::AbstractTimeSyncManager
{
public:
    void start() override {}
    void stop() override {}
    void resync() override {}

    std::chrono::milliseconds getSyncTime(bool* /*outIsTimeTakenFromInternet*/) const override
    {
        return m_timeValue;
    }

    void adjust(std::chrono::milliseconds value)
    {
        m_timeValue += value;
    }

private:
    std::chrono::milliseconds m_timeValue{};
};

class AggregatorTest: public ::testing::Test
{
public:
    AggregatorTest(): m_timeSyncManagerStub(new TimeSyncManagerStub)
    {
        m_syncTime.setTimeSyncManager(m_timeSyncManagerStub);
    }

    void adjustTime(std::chrono::milliseconds value)
    {
        m_timeSyncManagerStub->adjust(value);
    }

private:
    std::shared_ptr<TimeSyncManagerStub> m_timeSyncManagerStub;
    QnSyncTime m_syncTime;
};

TEST_F(AggregatorTest, newAggregatorHasNoAggregatedEvents)
{
    auto aggregator = makeAggregator();
    EXPECT_TRUE(aggregator.popEvents().empty());
    EXPECT_TRUE(aggregator.empty());
}

TEST_F(AggregatorTest, firstEventIsNotAggregated)
{
    auto aggregator = makeAggregator();
    auto event = makeEvent();

    ASSERT_FALSE(aggregator.aggregate(event));

    auto events = aggregator.popEvents();
    EXPECT_EQ(events.size(), 0);
    EXPECT_TRUE(aggregator.empty());

    adjustTime(kDefaultAggregationInterval);

    events = aggregator.popEvents();
    EXPECT_EQ(events.size(), 0);
    EXPECT_TRUE(aggregator.empty());
}

TEST_F(AggregatorTest, aggregationIntervalWorks)
{
    constexpr auto kAggregationInterval = 100ms;
    auto aggregator = makeAggregator(kAggregationInterval);
    auto event1 = makeEvent("a");

    aggregator.aggregate(event1);

    constexpr auto kDelay1 = 20ms;
    adjustTime(kDelay1);
    auto event2 = makeEvent("a");
    ASSERT_TRUE(aggregator.aggregate(event2));

    constexpr auto kDelay2 = 50ms;
    adjustTime(kDelay2);
    auto event3 = makeEvent("a");
    ASSERT_TRUE(aggregator.aggregate(event3));

    // Just to be sure that aggregation time is not elapsed.
    ASSERT_TRUE(qnSyncTime->currentTimePoint() - event1->timestamp() < kAggregationInterval);

    // There is no elapsed events as the events are existing less than the given interval.
    ASSERT_FALSE(aggregator.empty());
    auto events = aggregator.popEvents();
    ASSERT_TRUE(events.empty());
    ASSERT_FALSE(aggregator.empty());

    adjustTime(kAggregationInterval - kDelay1 - kDelay2);
    auto currentTimePoint = qnSyncTime->currentTimePoint();
    // The interval is enough to consider event1 creation time as elapsed.
    auto interval = currentTimePoint - event1->timestamp();
    EXPECT_TRUE(interval > currentTimePoint - event2->timestamp());
    EXPECT_TRUE(interval > currentTimePoint - event3->timestamp());

    // Event though event2 and event3 are existing less than the interval, they timestamps are
    // considered as elapsed because they were aggregated based on the timestamp of the event1.
    events = aggregator.popEvents();
    ASSERT_EQ(events.size(), 1);
    EXPECT_EQ(events.front().event, event2);
    EXPECT_EQ(events.front().count, 2);
    EXPECT_TRUE(aggregator.empty());

    ASSERT_TRUE(qnSyncTime->currentTimePoint() - event3->timestamp() < kAggregationInterval);
    // Must be aggregated since the difference between current time and event3
    // timestamp(the last aggregated event) is less than the aggregation interval.
    auto event4 = makeEvent("a");
    ASSERT_TRUE(aggregator.aggregate(event4));
    EXPECT_FALSE(aggregator.empty());

    adjustTime(kAggregationInterval);
    events = aggregator.popEvents();
    ASSERT_EQ(events.size(), 1);
    EXPECT_EQ(events.front().event, event4);
    EXPECT_EQ(events.front().count, 1);
    EXPECT_TRUE(aggregator.empty());
}

TEST_F(AggregatorTest, aggregatorHandlesDifferentEventsProperly)
{
    constexpr auto kAggregationInterval = 100ms;
    auto aggregator = makeAggregator(kAggregationInterval);

    auto eventA1 = makeEvent("a");
    ASSERT_FALSE(aggregator.aggregate(eventA1));

    auto eventB1 = makeEvent("b");
    ASSERT_FALSE(aggregator.aggregate(eventB1));

    auto eventA2 = makeEvent("a");
    ASSERT_TRUE(aggregator.aggregate(eventA2));
    auto eventA3 = makeEvent("a");
    ASSERT_TRUE(aggregator.aggregate(eventA3));

    adjustTime(20ms);

    auto eventB2 = makeEvent("b");
    ASSERT_TRUE(aggregator.aggregate(eventB2));
    auto eventB3 = makeEvent("b");
    ASSERT_TRUE(aggregator.aggregate(eventB3));

    adjustTime(kAggregationInterval);

    auto events = aggregator.popEvents();
    ASSERT_EQ(events.size(), 2);

    auto iterator = events.begin();
    EXPECT_EQ(iterator->event->objectName(), "a");
    EXPECT_EQ(iterator->count, 2);
    ++iterator;
    EXPECT_EQ(iterator->event->objectName(), "b");
    EXPECT_EQ(iterator->count, 2);

    EXPECT_TRUE(aggregator.empty());
}

} // namespace nx::vms::rules::test
