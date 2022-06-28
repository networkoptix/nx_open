// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/vms/rules/basic_event.h>
#include <nx/vms/rules/event_aggregator.h>

#include "test_event.h"

namespace nx::vms::rules::test {

using std::chrono::microseconds;

namespace {

EventPtr makeTimestampDependentEvent(microseconds timestamp)
{
    return QSharedPointer<TestTimestampDependentEvent>::create(timestamp);
}

} // namespace

TEST(EventAggregatorTest, eventAggregatorDelegatesToInitialEvent)
{
    auto event = EventPtr::create(microseconds::zero());
    auto aggregator = EventAggregatorPtr::create(event);

    aggregator->aggregate(event);

    EXPECT_EQ(event->timestamp(), aggregator->timestamp());
    EXPECT_EQ(event->type(), aggregator->type());
}

TEST(EventAggregatorTest, aggregationWorksProperly)
{
    auto aggregator = EventAggregatorPtr::create(makeTimestampDependentEvent(microseconds(1)));

    aggregator->aggregate(makeTimestampDependentEvent(microseconds(2)));
    aggregator->aggregate(makeTimestampDependentEvent(microseconds(3)));
    aggregator->aggregate(makeTimestampDependentEvent(microseconds(3)));

    EXPECT_EQ(aggregator->totalCount(), 4);
    EXPECT_EQ(aggregator->uniqueCount(), 3);
}

TEST(EventAggregatorTest, filtrationWorksProperly)
{
    auto aggregator = EventAggregatorPtr::create(makeTimestampDependentEvent(microseconds(1)));

    aggregator->aggregate(makeTimestampDependentEvent(microseconds(2)));
    aggregator->aggregate(makeTimestampDependentEvent(microseconds(3)));
    aggregator->aggregate(makeTimestampDependentEvent(microseconds(4)));

    const auto filter = [](const EventPtr& e) { return e->timestamp() > microseconds(2); };

    auto filtered = aggregator->filtered(filter);

    EXPECT_EQ(filtered->totalCount(), 2);
    EXPECT_EQ(filtered->timestamp(), microseconds(3));
}

} // nx::vms::rules::test
