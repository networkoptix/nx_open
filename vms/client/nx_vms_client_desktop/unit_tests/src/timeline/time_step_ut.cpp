// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <QtCore/QDateTime>
#include <QtCore/QTimeZone>

#include <ui/graphics/items/controls/time_step.h>

namespace nx::vms::client::desktop {
namespace test {

using namespace std::chrono;

class QnTimeStepTest: public ::testing::Test
{
public:
    const QnTimeStep step_3h{QnTimeStep::Hours, 1h, 3, 24, {}, {}, /*relative*/ false};

    const QTimeZone defaultTimeZone{"America/Los_Angeles"};
    const QDate defaultDate{2025, 1, 1};

    const QTimeZone zone_with_2_00_DST_transition{"America/Los_Angeles"};
    const QDate spring2hTransitionDate{2025, 3, 9};
    const QDate fall2hTransitionDate{2025, 11, 2};

    const QTimeZone zone_with_3_00_DST_transition{"Europe/Kyiv"};
    const QDate spring3hTransitionDate{2025, 3, 30};
    const QDate fall3hTransitionDate{2025, 10, 26};
};

// ------------------------------------------------------------------------------------------------
// Tests for 3-hour steps.
// More-than-1-hour steps is the most complicated scale due to DST transitions.

TEST_F(QnTimeStepTest, roundUp_3h_none)
{
    const QDateTime dt0(defaultDate, QTime(0, 0), defaultTimeZone);
    EXPECT_EQ(dt0, QDateTime::fromMSecsSinceEpoch(
        roundUp(milliseconds(dt0.toMSecsSinceEpoch()), step_3h, dt0.timeZone()).count(),
        dt0.timeZone()));

    const QDateTime dt1(defaultDate, QTime(3, 0), defaultTimeZone);
    EXPECT_EQ(dt1, QDateTime::fromMSecsSinceEpoch(
        roundUp(milliseconds(dt1.toMSecsSinceEpoch()), step_3h, dt1.timeZone()).count(),
        dt1.timeZone()));
}

TEST_F(QnTimeStepTest, roundUp_3h_normal)
{
    const QDateTime expectedRoundedUp(defaultDate, QTime(3, 0), defaultTimeZone);

    const QDateTime dt0(defaultDate, QTime(1, 0), defaultTimeZone);
    EXPECT_EQ(expectedRoundedUp, QDateTime::fromMSecsSinceEpoch(
        roundUp(milliseconds(dt0.toMSecsSinceEpoch()), step_3h, dt0.timeZone()).count(),
        dt0.timeZone()));

    const QDateTime dt1(defaultDate, QTime(0, 1), defaultTimeZone);
    EXPECT_EQ(expectedRoundedUp, QDateTime::fromMSecsSinceEpoch(
        roundUp(milliseconds(dt1.toMSecsSinceEpoch()), step_3h, dt1.timeZone()).count(),
        dt1.timeZone()));

    const QDateTime dt2(defaultDate, QTime(0, 0, 1), defaultTimeZone);
    EXPECT_EQ(expectedRoundedUp, QDateTime::fromMSecsSinceEpoch(
        roundUp(milliseconds(dt2.toMSecsSinceEpoch()), step_3h, dt2.timeZone()).count(),
        dt2.timeZone()));

    const QDateTime dt3(defaultDate, QTime(0, 0, 0, 1), defaultTimeZone);
    EXPECT_EQ(expectedRoundedUp, QDateTime::fromMSecsSinceEpoch(
        roundUp(milliseconds(dt3.toMSecsSinceEpoch()), step_3h, dt3.timeZone()).count(),
        dt3.timeZone()));
}

TEST_F(QnTimeStepTest, roundUp_3h_acrossSpringTransition)
{
    const QDateTime beforeTransition(
        spring3hTransitionDate, QTime(1, 0), zone_with_3_00_DST_transition);

    EXPECT_FALSE(beforeTransition.isDaylightTime());

    // There is no 3:00am time because of the DST transition.
    // We expect skipping all the way to 6:00am.
    const QDateTime expectedRoundedUpAfterTransition(
        spring3hTransitionDate, QTime(6, 0), zone_with_3_00_DST_transition);

    const QDateTime afterTransition = QDateTime::fromMSecsSinceEpoch(roundUp(milliseconds(
        beforeTransition.toMSecsSinceEpoch()), step_3h, beforeTransition.timeZone()).count(),
        beforeTransition.timeZone());

    EXPECT_TRUE(afterTransition.isDaylightTime());
    EXPECT_EQ(afterTransition, expectedRoundedUpAfterTransition);
}

TEST_F(QnTimeStepTest, roundUp_3h_acrossFallTransition)
{
    const QDateTime beforeTransition(
        fall3hTransitionDate, QTime(1, 0), zone_with_3_00_DST_transition);

    EXPECT_TRUE(beforeTransition.isDaylightTime());

    // There are two 3:00am times because of the DST transition.
    // We expect rounding up to the first one.
    const QDateTime expectedRoundedUpAfterTransition(
        fall3hTransitionDate, QTime(3, 0), zone_with_3_00_DST_transition,
        QDateTime::TransitionResolution::PreferBefore);

    const QDateTime afterTransition = QDateTime::fromMSecsSinceEpoch(roundUp(milliseconds(
        beforeTransition.toMSecsSinceEpoch()), step_3h, beforeTransition.timeZone()).count(),
        beforeTransition.timeZone());

    EXPECT_TRUE(afterTransition.isDaylightTime());
    EXPECT_EQ(afterTransition, expectedRoundedUpAfterTransition);
}

TEST_F(QnTimeStepTest, add_3h_normal)
{
    const QDateTime dt(defaultDate, QTime(0, 0), defaultTimeZone);
    const QDateTime expected(defaultDate, QTime(3, 0), defaultTimeZone);

    EXPECT_EQ(expected, QDateTime::fromMSecsSinceEpoch(
        add(milliseconds(dt.toMSecsSinceEpoch()), step_3h, dt.timeZone()).count(),
        dt.timeZone()));
}

TEST_F(QnTimeStepTest, add_3h_notRounded)
{
    const QDateTime dt(defaultDate, QTime(1, 0), defaultTimeZone);
    const QDateTime expected(defaultDate, QTime(4, 0), defaultTimeZone);

    EXPECT_EQ(expected, QDateTime::fromMSecsSinceEpoch(
        add(milliseconds(dt.toMSecsSinceEpoch()), step_3h, dt.timeZone()).count(),
        dt.timeZone()));
}

TEST_F(QnTimeStepTest, add_3h_nextDay)
{
    const QDateTime dt(defaultDate, QTime(21, 0), defaultTimeZone);
    const QDateTime expected(defaultDate.addDays(1), QTime(0, 0), defaultTimeZone);

    EXPECT_EQ(expected, QDateTime::fromMSecsSinceEpoch(
        add(milliseconds(dt.toMSecsSinceEpoch()), step_3h, dt.timeZone()).count(),
        dt.timeZone()));
}

TEST_F(QnTimeStepTest, add_3h_notRoundedAcrossDay)
{
    const QDateTime dt(defaultDate, QTime(22, 0), defaultTimeZone);
    const QDateTime expected(defaultDate.addDays(1), QTime(1, 0), defaultTimeZone);

    EXPECT_EQ(expected, QDateTime::fromMSecsSinceEpoch(
        add(milliseconds(dt.toMSecsSinceEpoch()), step_3h, dt.timeZone()).count(),
        dt.timeZone()));
}

TEST_F(QnTimeStepTest, add_3h_across2hSpringTransition)
{
    const QDateTime beforeTransition(
        spring2hTransitionDate, QTime(0, 0), zone_with_2_00_DST_transition);

    EXPECT_FALSE(beforeTransition.isDaylightTime());

    const QDateTime expected(spring2hTransitionDate, QTime(3, 0), zone_with_2_00_DST_transition);

    const QDateTime result = QDateTime::fromMSecsSinceEpoch(add(milliseconds(
        beforeTransition.toMSecsSinceEpoch()), step_3h, beforeTransition.timeZone()).count(),
        beforeTransition.timeZone());

    EXPECT_TRUE(result.isDaylightTime());
    EXPECT_EQ(expected, result);
}

TEST_F(QnTimeStepTest, add_3h_across2hFallTransition)
{
    const QDateTime beforeTransition(
        fall2hTransitionDate, QTime(0, 0), zone_with_2_00_DST_transition);

    EXPECT_TRUE(beforeTransition.isDaylightTime());

    const QDateTime expected(fall2hTransitionDate, QTime(3, 0), zone_with_2_00_DST_transition);

    const QDateTime result = QDateTime::fromMSecsSinceEpoch(add(milliseconds(
        beforeTransition.toMSecsSinceEpoch()), step_3h, beforeTransition.timeZone()).count(),
        beforeTransition.timeZone());

    EXPECT_FALSE(result.isDaylightTime());
    EXPECT_EQ(expected, result);
}

TEST_F(QnTimeStepTest, add_3h_across3hSpringTransition)
{
    const QDateTime beforeTransition(
        spring3hTransitionDate, QTime(0, 0), zone_with_3_00_DST_transition);

    EXPECT_FALSE(beforeTransition.isDaylightTime());

    // There is no 3:00am time because of the DST transition.
    // We expect skipping all the way to 6:00am.
    const QDateTime expected(spring3hTransitionDate, QTime(6, 0), zone_with_3_00_DST_transition);

    const QDateTime result = QDateTime::fromMSecsSinceEpoch(add(milliseconds(
        beforeTransition.toMSecsSinceEpoch()), step_3h, beforeTransition.timeZone()).count(),
        beforeTransition.timeZone());

    EXPECT_TRUE(result.isDaylightTime());
    EXPECT_EQ(expected, result);
}

TEST_F(QnTimeStepTest, add_3h_across3hFallTransition)
{
    const QDateTime beforeTransition(
        fall3hTransitionDate, QTime(0, 0), zone_with_3_00_DST_transition);

    EXPECT_TRUE(beforeTransition.isDaylightTime());

    const QDateTime expected1st(fall3hTransitionDate, QTime(3, 0), zone_with_3_00_DST_transition,
        QDateTime::TransitionResolution::PreferBefore);

    const QDateTime expected2nd(fall3hTransitionDate, QTime(6, 0), zone_with_3_00_DST_transition);

    const QDateTime result1st = QDateTime::fromMSecsSinceEpoch(add(milliseconds(
        beforeTransition.toMSecsSinceEpoch()), step_3h, beforeTransition.timeZone()).count(),
        beforeTransition.timeZone());

    const QDateTime result2nd = QDateTime::fromMSecsSinceEpoch(add(milliseconds(
        result1st.toMSecsSinceEpoch()), step_3h, beforeTransition.timeZone()).count(),
        beforeTransition.timeZone());

    EXPECT_TRUE(result1st.isDaylightTime());
    EXPECT_FALSE(result2nd.isDaylightTime());

    EXPECT_EQ(expected1st, result1st);
    EXPECT_EQ(expected2nd, result2nd);
}

TEST_F(QnTimeStepTest, sub_3h_normal)
{
    const QDateTime dt(defaultDate, QTime(3, 0), defaultTimeZone);
    const QDateTime expected(defaultDate, QTime(0, 0), defaultTimeZone);

    EXPECT_EQ(expected, QDateTime::fromMSecsSinceEpoch(
        sub(milliseconds(dt.toMSecsSinceEpoch()), step_3h, dt.timeZone()).count(),
        dt.timeZone()));
}

TEST_F(QnTimeStepTest, sub_3h_notRounded)
{
    const QDateTime dt(defaultDate, QTime(4, 0), defaultTimeZone);
    const QDateTime expected(defaultDate, QTime(1, 0), defaultTimeZone);

    EXPECT_EQ(expected, QDateTime::fromMSecsSinceEpoch(
        sub(milliseconds(dt.toMSecsSinceEpoch()), step_3h, dt.timeZone()).count(),
        dt.timeZone()));
}

TEST_F(QnTimeStepTest, sub_3h_prevDay)
{
    const QDateTime dt(defaultDate, QTime(0, 0), defaultTimeZone);
    const QDateTime expected(defaultDate.addDays(-1), QTime(21, 0), defaultTimeZone);

    EXPECT_EQ(expected, QDateTime::fromMSecsSinceEpoch(
        sub(milliseconds(dt.toMSecsSinceEpoch()), step_3h, dt.timeZone()).count(),
        dt.timeZone()));
}

TEST_F(QnTimeStepTest, sub_3h_notRoundedAcrossDay)
{
    const QDateTime dt(defaultDate, QTime(1, 0), defaultTimeZone);
    const QDateTime expected(defaultDate.addDays(-1), QTime(22, 0), defaultTimeZone);

    EXPECT_EQ(expected, QDateTime::fromMSecsSinceEpoch(
        sub(milliseconds(dt.toMSecsSinceEpoch()), step_3h, dt.timeZone()).count(),
        dt.timeZone()));
}

TEST_F(QnTimeStepTest, sub_3h_across2hSpringTransition)
{
    const QDateTime beforeTransition(
        spring2hTransitionDate, QTime(3, 0), zone_with_2_00_DST_transition);

    EXPECT_TRUE(beforeTransition.isDaylightTime());

    const QDateTime expected(spring2hTransitionDate, QTime(0, 0), zone_with_2_00_DST_transition);

    const QDateTime result = QDateTime::fromMSecsSinceEpoch(sub(milliseconds(
        beforeTransition.toMSecsSinceEpoch()), step_3h, beforeTransition.timeZone()).count(),
        beforeTransition.timeZone());

    EXPECT_FALSE(result.isDaylightTime());
    EXPECT_EQ(expected, result);
}

TEST_F(QnTimeStepTest, sub_3h_across2hFallTransition)
{
    const QDateTime beforeTransition(
        fall2hTransitionDate, QTime(3, 0), zone_with_2_00_DST_transition);

    EXPECT_FALSE(beforeTransition.isDaylightTime());

    const QDateTime expected(fall2hTransitionDate, QTime(0, 0), zone_with_2_00_DST_transition);

    const QDateTime result = QDateTime::fromMSecsSinceEpoch(sub(milliseconds(
        beforeTransition.toMSecsSinceEpoch()), step_3h, beforeTransition.timeZone()).count(),
        beforeTransition.timeZone());

    EXPECT_TRUE(result.isDaylightTime());
    EXPECT_EQ(expected, result);
}

TEST_F(QnTimeStepTest, sub_3h_acrossSpringTransition)
{
    const QDateTime beforeTransition(
        spring3hTransitionDate, QTime(6, 0), zone_with_3_00_DST_transition);

    EXPECT_TRUE(beforeTransition.isDaylightTime());

    // There is no 3:00am time because of the DST transition.
    // We expect skipping all the way to 0:00am.
    const QDateTime expected(spring3hTransitionDate, QTime(0, 0), zone_with_3_00_DST_transition);

    const QDateTime result = QDateTime::fromMSecsSinceEpoch(sub(milliseconds(
        beforeTransition.toMSecsSinceEpoch()), step_3h, beforeTransition.timeZone()).count(),
        beforeTransition.timeZone());

    EXPECT_FALSE(result.isDaylightTime());
    EXPECT_EQ(expected, result);
}

TEST_F(QnTimeStepTest, sub_3h_acrossFallTransition)
{
    const QDateTime beforeTransition(
        fall3hTransitionDate, QTime(6, 0), zone_with_3_00_DST_transition);

    EXPECT_FALSE(beforeTransition.isDaylightTime());

    const QDateTime expected1st(fall3hTransitionDate, QTime(3, 0), zone_with_3_00_DST_transition,
        QDateTime::TransitionResolution::PreferBefore);

    const QDateTime expected2nd(fall3hTransitionDate, QTime(0, 0), zone_with_3_00_DST_transition);

    const QDateTime result1st = QDateTime::fromMSecsSinceEpoch(sub(milliseconds(
        beforeTransition.toMSecsSinceEpoch()), step_3h, beforeTransition.timeZone()).count(),
        beforeTransition.timeZone());

    const QDateTime result2nd = QDateTime::fromMSecsSinceEpoch(sub(milliseconds(
        result1st.toMSecsSinceEpoch()), step_3h, beforeTransition.timeZone()).count(),
        beforeTransition.timeZone());

    EXPECT_TRUE(result1st.isDaylightTime());
    EXPECT_TRUE(result2nd.isDaylightTime());

    EXPECT_EQ(expected1st, result1st);
    EXPECT_EQ(expected2nd, result2nd);
}

} // namespace test
} // namespace nx::vms::client::desktop
