// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <nx/vms/rules/aggregated_event.h>

#include "test_event.h"

namespace nx::vms::rules::test {

class TestEventWithCounter: public TestEvent
{
public:
    TestEventWithCounter(size_t counter = 0): m_counter(counter) {}

    virtual QString uniqueName() const override
    {
        return makeName(TestEvent::uniqueName(), QString::number(m_counter));
    }

private:
    size_t m_counter = 0;
};

TEST(AggregatedEventTest, defaultAggregatedEventCreatedCorrectly)
{
    AggregatedEvent aggregatedEvent;
    ASSERT_TRUE(aggregatedEvent.empty());
    ASSERT_EQ(aggregatedEvent.uniqueEventCount(), 0);
    ASSERT_EQ(aggregatedEvent.totalEventCount(), 0);
}

TEST(AggregatedEventTest, aggregatedEventInitialisedProperly)
{
    AggregatedEvent aggregatedEvent;
    EventPtr testEventWithCounter(new TestEventWithCounter);

    aggregatedEvent.aggregate(testEventWithCounter);

    ASSERT_FALSE(aggregatedEvent.empty());
    ASSERT_EQ(aggregatedEvent.type(), testEventWithCounter->type());
    ASSERT_EQ(aggregatedEvent.timestamp(), testEventWithCounter->timestamp());
    ASSERT_EQ(aggregatedEvent.initialEvent(), testEventWithCounter);
    ASSERT_EQ(aggregatedEvent.uniqueEventCount(), 1);
    ASSERT_EQ(aggregatedEvent.totalEventCount(), 1);
}

TEST(AggregatedEventTest, eventAggregationWorksProperly)
{
    using namespace std::chrono_literals;

    AggregatedEvent aggregatedEvent;

    EventPtr testEventWithCounter0(new TestEventWithCounter(0));
    testEventWithCounter0->setTimestamp(1us);

    aggregatedEvent.aggregate(testEventWithCounter0);
    ASSERT_EQ(aggregatedEvent.uniqueEventCount(), 1);
    ASSERT_EQ(aggregatedEvent.totalEventCount(), 1);

    aggregatedEvent.aggregate(testEventWithCounter0);
    ASSERT_EQ(aggregatedEvent.uniqueEventCount(), 1);
    ASSERT_EQ(aggregatedEvent.totalEventCount(), 2);

    EventPtr testEventWithCounter1(new TestEventWithCounter(1));
    testEventWithCounter1->setTimestamp(2us);
    aggregatedEvent.aggregate(testEventWithCounter1);
    ASSERT_EQ(aggregatedEvent.uniqueEventCount(), 2);
    ASSERT_EQ(aggregatedEvent.totalEventCount(), 3);

    auto events = aggregatedEvent.events();
    ASSERT_EQ(events.size(), aggregatedEvent.uniqueEventCount());
    ASSERT_EQ(events[0], testEventWithCounter0);
    ASSERT_EQ(events[1], testEventWithCounter1);
}

TEST(AggregatedEventTest, resetAggregatedEventsWorksProperly)
{
    AggregatedEvent aggregatedEvent;

    EventPtr testEventWithCounter(new TestEventWithCounter);

    aggregatedEvent.aggregate(testEventWithCounter);

    ASSERT_FALSE(aggregatedEvent.empty());

    aggregatedEvent.reset();

    ASSERT_TRUE(aggregatedEvent.empty());
    ASSERT_EQ(aggregatedEvent.type(), QString());
    ASSERT_EQ(aggregatedEvent.timestamp(), std::chrono::microseconds());
    ASSERT_EQ(aggregatedEvent.uniqueEventCount(), 0);
    ASSERT_EQ(aggregatedEvent.totalEventCount(), 0);
}

} // namespace nx::vms::rules::test
