// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/vms/event/event_parameters.h>
#include <nx/vms/rules/action_fields/event_id_field.h>
#include <nx/vms/rules/aggregated_event.h>

#include "test_event.h"

namespace nx::vms::rules::test {

using namespace nx::vms::rules;

namespace {

QnUuid buildEventId(const EventPtr& event)
{
    auto aggEvent = AggregatedEventPtr::create(event);
    auto field = QSharedPointer<EventIdField>::create();

    return field->build(aggEvent).value<QnUuid>();
}

} // namespace

TEST(EventIdField, Equality)
{
    const auto params = nx::vms::event::EventParameters{
        .eventTimestampUsec = 42,
        .eventResourceId = QnUuid::createUuid(),
    };

    const auto serverEvent = QSharedPointer<TestEvent>::create();
    serverEvent->setTimestamp(std::chrono::microseconds(params.eventTimestampUsec));
    serverEvent->m_serverId = params.eventResourceId;

    ASSERT_FALSE(params.getUniqueId().isNull());
    EXPECT_EQ(params.getUniqueId(), buildEventId(serverEvent));

    const auto cameraEvent = QSharedPointer<TestEvent>::create();
    cameraEvent->setTimestamp(std::chrono::microseconds(params.eventTimestampUsec));
    cameraEvent->m_cameraId = params.eventResourceId;

    ASSERT_FALSE(params.getUniqueId().isNull());
    EXPECT_EQ(params.getUniqueId(), buildEventId(cameraEvent));

    auto paramsTime = params;
    ++paramsTime.eventTimestampUsec;
    ASSERT_FALSE(paramsTime.getUniqueId().isNull());
    EXPECT_NE(paramsTime.getUniqueId(), buildEventId(serverEvent));

    auto paramsRes = params;
    paramsRes.eventResourceId = QnUuid::createUuid();
    ASSERT_FALSE(paramsRes.getUniqueId().isNull());
    EXPECT_NE(paramsRes.getUniqueId(), buildEventId(serverEvent));
    EXPECT_NE(paramsRes.getUniqueId(), paramsTime.getUniqueId());
}

} // namespace nx::vms::rules::test
