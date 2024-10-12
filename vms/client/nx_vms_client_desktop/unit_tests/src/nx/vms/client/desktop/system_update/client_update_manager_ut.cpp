// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <QtCore/QTimeZone>

#include <nx/vms/client/desktop/system_update/client_update_manager.h>
#include <nx/vms/common/update/update_information.h>

using namespace std::chrono;

std::ostream& operator<<(std::ostream& stream, const QDateTime& dateTime)
{
    return stream << dateTime.toString().toStdString();
}

namespace nx::vms::client::desktop::test {

common::update::Information makeInfo(milliseconds releaseDate, int deliveryDays)
{
    // We don't other fields to test update planning.
    return common::update::Information{update::PublicationInfo{
        .releaseDate = releaseDate,
        .releaseDeliveryDays = deliveryDays}};
}

QDateTime dt(const milliseconds& timestamp) //< To make failed test output prettier.
{
    return QDateTime::fromMSecsSinceEpoch(timestamp.count(), QTimeZone::UTC);
}

const milliseconds kDay = 24h;
const milliseconds kSunday = sys_days(January/8/2023).time_since_epoch();
const milliseconds kMonday = kSunday + kDay;
const milliseconds kTuesday = kMonday + kDay;
const milliseconds kWednesday = kTuesday + kDay;
const milliseconds kThursday = kWednesday + kDay;
const milliseconds kFriday = kThursday + kDay;
const milliseconds kSaturday = kFriday + kDay;
const milliseconds kNextSunday = kSaturday + kDay;

TEST(ClientUpdateManagerTest, trivialPlanning)
{
    const auto date = ClientUpdateManager::calculateUpdateDate(
        kMonday, makeInfo(kTuesday, 1), {}, {});

    EXPECT_GE(dt(date.date), dt(kTuesday));
    EXPECT_LE(dt(date.date), dt(kWednesday));
    EXPECT_GE(date.shift, 0ms);
    EXPECT_LE(date.shift, kDay);
}

TEST(ClientUpdateManagerTest, releaseDatePassed)
{
    const auto date = ClientUpdateManager::calculateUpdateDate(
        kWednesday, makeInfo(kSunday, 1), {}, {});

    EXPECT_EQ(dt(date.date), dt(kWednesday));
    EXPECT_GE(date.shift, 0ms);
    EXPECT_LE(date.shift, kDay);
}

TEST(ClientUpdateManagerTest, releaseDatePassedRecently)
{
    const auto date = ClientUpdateManager::calculateUpdateDate(
        kMonday, makeInfo(kMonday - 3h, 1), {}, {});

    EXPECT_GE(dt(date.date), dt(kMonday));
    EXPECT_GE(date.shift, 0ms);
    EXPECT_LE(date.shift, kDay);
}

TEST(ClientUpdateManagerTest, updateInTheEndOfWeek)
{
    const auto date = ClientUpdateManager::calculateUpdateDate(
        kMonday, makeInfo(kFriday, 1), {}, {});

    EXPECT_GE(dt(date.date), dt(kNextSunday));
    EXPECT_GE(date.shift, 0ms);
    EXPECT_LE(date.shift, kDay);
}

TEST(ClientUpdateManagerTest, updateInTheEndOfWeekWhenReleaseDatePassed)
{
    const auto date = ClientUpdateManager::calculateUpdateDate(
        kFriday, makeInfo(kThursday, 1), {}, {});

    EXPECT_GE(dt(date.date), dt(kNextSunday));
    EXPECT_GE(date.shift, 0ms);
    EXPECT_LE(date.shift, kDay);
}

TEST(ClientUpdateManagerTest, justATest)
{
    const auto date = ClientUpdateManager::calculateUpdateDate(
        sys_days(February/20/2023).time_since_epoch(),
        makeInfo(sys_days(February/22/2023).time_since_epoch(), 1), {}, {});

    EXPECT_GE(dt(date.date), dt(sys_days(February/22/2023).time_since_epoch()));
    EXPECT_GE(date.shift, 0ms);
    EXPECT_LE(date.shift, kDay);
}

} // namespace nx::vms::client::desktop::test
