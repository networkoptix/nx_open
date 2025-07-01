// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <optional>

#include <gtest/gtest.h>

#include <QtCore/QString>

#include <nx/utils/uuid.h>
#include <nx/vms/rules/rules_fwd.h>

namespace nx::vms::common { class SystemContext; }

namespace nx::vms::rules::test {

class EventDetailsTestBase: public ::testing::Test
{
protected:
    static const QString kSystemName;
    static const QString kSystemSignature;

    static const QString kServerName;
    static const Uuid kServerId;

    static const QString kServer2Name;
    static const Uuid kServer2Id;

    static const QString kEngine1Name;
    static const Uuid kEngine1Id;

    static const QString kCamera1Name;
    static const Uuid kCamera1Id;
    static const QString kCamera1PhysicalId;

    static const QString kCamera2Name;
    static const Uuid kCamera2Id;
    static const QString kCamera2PhysicalId;

    static const QString kUser1Name;
    static const Uuid kUser1Id;

    static const QString kUser2Name;
    static const Uuid kUser2Id;

    static void SetUpTestSuite();
    static void TearDownTestSuite();
    static common::SystemContext* systemContext();

    void givenEvent(EventPtr event);

    void whenEventsLimitSetTo(int value);

    AggregatedEventPtr buildEvent();

private:
    void ensureAggregationIsValid() const;

private:
    std::vector<EventPtr> m_events;
    std::optional<int> m_eventLimitOverload;
    AggregatedEventPtr m_aggregatedEvent;
};

} // namespace nx::vms::rules::test
