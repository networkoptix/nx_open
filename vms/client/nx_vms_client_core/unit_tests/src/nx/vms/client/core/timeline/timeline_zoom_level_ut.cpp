// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <chrono>

#include <QtCore/QDateTime>
#include <QtCore/QTimeZone>

#include <nx/vms/client/core/timeline/data/timeline_zoom_level.h>

std::ostream& operator<<(std::ostream& stream, const QDateTime& dateTime)
{
    return stream << dateTime.toString().toStdString();
}

namespace nx::vms::client::core {
namespace test {

using namespace std::chrono;

static const QTimeZone kTestTimeZone("America/Los_Angeles");

TEST(TimelineZoomLevel, alignHoursNearTransition1)
{
    const auto tickStep = 3h;
    const QDate transitionDate(2025, 11, 2); //< Fall back from PDT to PST at 2:00am.
    const TimelineZoomLevel zoom{TimelineZoomLevel::Hours, milliseconds(tickStep).count()};

    const QDateTime startOfDay(transitionDate, QTime(0, 0), kTestTimeZone);

    // 0:00
    const qint64 aligned0 = zoom.alignTick(startOfDay.toMSecsSinceEpoch(), kTestTimeZone);
    EXPECT_EQ(QDateTime::fromMSecsSinceEpoch(aligned0, kTestTimeZone),
        startOfDay);

    // First 1:00
    const qint64 aligned1 = zoom.alignTick(startOfDay.addDuration(1h).toMSecsSinceEpoch(),
        kTestTimeZone);
    EXPECT_EQ(QDateTime::fromMSecsSinceEpoch(aligned1, kTestTimeZone),
        startOfDay);

    // Second 1:00
    const qint64 aligned1b = zoom.alignTick(startOfDay.addDuration(2h).toMSecsSinceEpoch(),
        kTestTimeZone);
    EXPECT_EQ(QDateTime::fromMSecsSinceEpoch(aligned1b, kTestTimeZone),
        startOfDay);

    // 2:00
    const qint64 aligned2 = zoom.alignTick(startOfDay.addDuration(3h).toMSecsSinceEpoch(),
        kTestTimeZone);
    EXPECT_EQ(QDateTime::fromMSecsSinceEpoch(aligned2, kTestTimeZone),
        startOfDay);

    // 3:00
    const qint64 aligned3 = zoom.alignTick(startOfDay.addDuration(4h).toMSecsSinceEpoch(),
        kTestTimeZone);
    EXPECT_EQ(QDateTime::fromMSecsSinceEpoch(aligned3, kTestTimeZone),
        QDateTime(transitionDate, QTime(3, 0), kTestTimeZone));

    // 20:00
    const qint64 aligned20 = zoom.alignTick(startOfDay.addDuration(21h).toMSecsSinceEpoch(),
        kTestTimeZone);
    EXPECT_EQ(QDateTime::fromMSecsSinceEpoch(aligned20, kTestTimeZone),
        QDateTime(transitionDate, QTime(18, 0), kTestTimeZone));

    // 21:00
    const qint64 aligned21 = zoom.alignTick(startOfDay.addDuration(22h).toMSecsSinceEpoch(),
        kTestTimeZone);
    EXPECT_EQ(QDateTime::fromMSecsSinceEpoch(aligned21, kTestTimeZone),
        QDateTime(transitionDate, QTime(21, 0), kTestTimeZone));

    // 23:00
    const qint64 aligned23 = zoom.alignTick(startOfDay.addDuration(24h).toMSecsSinceEpoch(),
        kTestTimeZone);
    EXPECT_EQ(QDateTime::fromMSecsSinceEpoch(aligned23, kTestTimeZone),
        QDateTime(transitionDate, QTime(21, 0), kTestTimeZone));
}

TEST(TimelineZoomLevel, alignHoursNearTransition2)
{
    const auto tickStep = 3h;
    const QDate transitionDate(2025, 3, 9); //< Spring forward from PST to PDT at 2:00am.
    const TimelineZoomLevel zoom{TimelineZoomLevel::Hours, milliseconds(tickStep).count()};

    const QDateTime startOfDay(transitionDate, QTime(0, 0), kTestTimeZone);

    // 0:00
    const qint64 aligned0 = zoom.alignTick(startOfDay.toMSecsSinceEpoch(), kTestTimeZone);
    EXPECT_EQ(QDateTime::fromMSecsSinceEpoch(aligned0, kTestTimeZone),
        startOfDay);

    // 1:00
    const qint64 aligned1 = zoom.alignTick(startOfDay.addDuration(1h).toMSecsSinceEpoch(),
        kTestTimeZone);
    EXPECT_EQ(QDateTime::fromMSecsSinceEpoch(aligned1, kTestTimeZone),
        startOfDay);

    // 3:00
    const qint64 aligned3 = zoom.alignTick(startOfDay.addDuration(2h).toMSecsSinceEpoch(),
        kTestTimeZone);
    EXPECT_EQ(QDateTime::fromMSecsSinceEpoch(aligned3, kTestTimeZone),
        QDateTime(transitionDate, QTime(3, 0), kTestTimeZone));

    // 4:00
    const qint64 aligned4 = zoom.alignTick(startOfDay.addDuration(3h).toMSecsSinceEpoch(),
        kTestTimeZone);
    EXPECT_EQ(QDateTime::fromMSecsSinceEpoch(aligned4, kTestTimeZone),
        QDateTime(transitionDate, QTime(3, 0), kTestTimeZone));

    // 20:00
    const qint64 aligned20 = zoom.alignTick(startOfDay.addDuration(19h).toMSecsSinceEpoch(),
        kTestTimeZone);
    EXPECT_EQ(QDateTime::fromMSecsSinceEpoch(aligned20, kTestTimeZone),
        QDateTime(transitionDate, QTime(18, 0), kTestTimeZone));

    // 21:00
    const qint64 aligned21 = zoom.alignTick(startOfDay.addDuration(20h).toMSecsSinceEpoch(),
        kTestTimeZone);
    EXPECT_EQ(QDateTime::fromMSecsSinceEpoch(aligned21, kTestTimeZone),
        QDateTime(transitionDate, QTime(21, 0), kTestTimeZone));

    // 23:00
    const qint64 aligned23 = zoom.alignTick(startOfDay.addDuration(22h).toMSecsSinceEpoch(),
        kTestTimeZone);
    EXPECT_EQ(QDateTime::fromMSecsSinceEpoch(aligned23, kTestTimeZone),
        QDateTime(transitionDate, QTime(21, 0), kTestTimeZone));
}

TEST(TimelineZoomLevel, nextStepNearTransition1)
{
    const auto tickStep = 6h;
    const QDate transitionDate(2025, 11, 2); //< Fall back from PDT to PST at 2:00am.
    const TimelineZoomLevel zoom{TimelineZoomLevel::Hours, milliseconds(tickStep).count()};

    const QDateTime startOfDay(transitionDate, QTime(0, 0), kTestTimeZone);

    qint64 next = zoom.nextTick(startOfDay.toMSecsSinceEpoch(), kTestTimeZone);
    EXPECT_EQ(QDateTime::fromMSecsSinceEpoch(next, kTestTimeZone),
        QDateTime(transitionDate, QTime(6, 0), kTestTimeZone));

    next = zoom.nextTick(next, kTestTimeZone);
    EXPECT_EQ(QDateTime::fromMSecsSinceEpoch(next, kTestTimeZone),
        QDateTime(transitionDate, QTime(12, 0), kTestTimeZone));

    next = zoom.nextTick(next, kTestTimeZone);
    EXPECT_EQ(QDateTime::fromMSecsSinceEpoch(next, kTestTimeZone),
        QDateTime(transitionDate, QTime(18, 0), kTestTimeZone));

    next = zoom.nextTick(next, kTestTimeZone);
    EXPECT_EQ(QDateTime::fromMSecsSinceEpoch(next, kTestTimeZone),
        QDateTime(transitionDate.addDays(1), QTime(0, 0), kTestTimeZone));
}

TEST(TimelineZoomLevel, nextStepNearTransition2)
{
    const auto tickStep = 6h;
    const QDate transitionDate(2025, 3, 9); //< Spring forward from PST to PDT at 2:00am.
    const TimelineZoomLevel zoom{TimelineZoomLevel::Hours, milliseconds(tickStep).count()};

    const QDateTime startOfDay(transitionDate, QTime(0, 0), kTestTimeZone);

    qint64 next = zoom.nextTick(startOfDay.toMSecsSinceEpoch(), kTestTimeZone);
    EXPECT_EQ(QDateTime::fromMSecsSinceEpoch(next, kTestTimeZone),
        QDateTime(transitionDate, QTime(6, 0), kTestTimeZone));

    next = zoom.nextTick(next, kTestTimeZone);
    EXPECT_EQ(QDateTime::fromMSecsSinceEpoch(next, kTestTimeZone),
        QDateTime(transitionDate, QTime(12, 0), kTestTimeZone));

    next = zoom.nextTick(next, kTestTimeZone);
    EXPECT_EQ(QDateTime::fromMSecsSinceEpoch(next, kTestTimeZone),
        QDateTime(transitionDate, QTime(18, 0), kTestTimeZone));

    next = zoom.nextTick(next, kTestTimeZone);
    EXPECT_EQ(QDateTime::fromMSecsSinceEpoch(next, kTestTimeZone),
        QDateTime(transitionDate.addDays(1), QTime(0, 0), kTestTimeZone));
}

TEST(TimelineZoomLevel, nextStepNearTransition3)
{
    const auto tickStep = 2h; //< To hit non-existant "2:00am"
    const QDate transitionDate(2025, 3, 9); //< Spring forward from PST to PDT at 2:00am.
    const TimelineZoomLevel zoom{TimelineZoomLevel::Hours, milliseconds(tickStep).count()};

    const QDateTime startOfDay(transitionDate, QTime(0, 0), kTestTimeZone);

    qint64 next = zoom.nextTick(startOfDay.toMSecsSinceEpoch(), kTestTimeZone);
    EXPECT_EQ(QDateTime::fromMSecsSinceEpoch(next, kTestTimeZone),
        QDateTime(transitionDate, QTime(3, 0), kTestTimeZone));

    next = zoom.nextTick(next, kTestTimeZone);
    EXPECT_EQ(QDateTime::fromMSecsSinceEpoch(next, kTestTimeZone),
        QDateTime(transitionDate, QTime(4, 0), kTestTimeZone));

    next = zoom.nextTick(next, kTestTimeZone);
    EXPECT_EQ(QDateTime::fromMSecsSinceEpoch(next, kTestTimeZone),
        QDateTime(transitionDate, QTime(6, 0), kTestTimeZone));
}

TEST(TimelineZoomLevel, monotonousNextStepNearTransition1)
{
    const auto tickStep = 1h;
    const QDate transitionDate(2025, 11, 2); //< Fall back from PDT to PST at 2:00am.
    const TimelineZoomLevel zoom{TimelineZoomLevel::Hours, milliseconds(tickStep).count()};

    const QDateTime startOfDay(transitionDate, QTime(0, 0), kTestTimeZone);

    qint64 next = zoom.nextTick(startOfDay.toMSecsSinceEpoch(), kTestTimeZone);
    EXPECT_EQ(QDateTime::fromMSecsSinceEpoch(next, kTestTimeZone),
        startOfDay.addDuration(1h));

    next = zoom.nextTick(next, kTestTimeZone);
    EXPECT_EQ(QDateTime::fromMSecsSinceEpoch(next, kTestTimeZone),
        startOfDay.addDuration(2h));

    next = zoom.nextTick(next, kTestTimeZone);
    EXPECT_EQ(QDateTime::fromMSecsSinceEpoch(next, kTestTimeZone),
        QDateTime(transitionDate, QTime(2, 0), kTestTimeZone));

    next = zoom.nextTick(next, kTestTimeZone);
    EXPECT_EQ(QDateTime::fromMSecsSinceEpoch(next, kTestTimeZone),
        QDateTime(transitionDate, QTime(3, 0), kTestTimeZone));
}

TEST(TimelineZoomLevel, monotonousNextStepNearTransition2)
{
    const auto tickStep = 1h;
    const QDate transitionDate(2025, 3, 9); //< Spring forward from PST to PDT at 2:00am.
    const TimelineZoomLevel zoom{TimelineZoomLevel::Hours, milliseconds(tickStep).count()};

    const QDateTime startOfDay(transitionDate, QTime(0, 0), kTestTimeZone);

    qint64 next = zoom.nextTick(startOfDay.toMSecsSinceEpoch(), kTestTimeZone);
    EXPECT_EQ(QDateTime::fromMSecsSinceEpoch(next, kTestTimeZone),
        QDateTime(transitionDate, QTime(1, 0), kTestTimeZone));

    next = zoom.nextTick(next, kTestTimeZone);
    EXPECT_EQ(QDateTime::fromMSecsSinceEpoch(next, kTestTimeZone),
        QDateTime(transitionDate, QTime(3, 0), kTestTimeZone));

    next = zoom.nextTick(next, kTestTimeZone);
    EXPECT_EQ(QDateTime::fromMSecsSinceEpoch(next, kTestTimeZone),
        QDateTime(transitionDate, QTime(4, 0), kTestTimeZone));
}

} // namespace test
} // namespace nx::vms::client::core
