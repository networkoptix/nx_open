#include <gtest/gtest.h>

#include <nx/utils/elapsed_timer.h>

namespace nx::utils::test {

using namespace std::chrono_literals;

TEST(ElapsedTimer, Base)
{
    ScopedTimeShift timeShift(nx::utils::test::ClockType::steady);

    auto timer = ElapsedTimer();
    ASSERT_EQ(timer.hasExpired(0ms), true);
    ASSERT_EQ(timer.hasExpired(7s), true);
    ASSERT_EQ(timer.isValid(), false);

    timeShift.applyRelativeShift(1s);
    timer.invalidate();
    ASSERT_EQ(timer.hasExpired(0ms), true);
    ASSERT_EQ(timer.hasExpired(7s), true);
    ASSERT_EQ(timer.isValid(), false);

    auto restartValue = timer.restart();
    ASSERT_EQ(restartValue, 0ms);
    ASSERT_EQ(timer.hasExpired(7s), false);
    ASSERT_EQ(timer.isValid(), true);

    timeShift.applyRelativeShift(77s);
    ASSERT_EQ(timer.hasExpired(777s), false);
    ASSERT_EQ(timer.hasExpired(7s), true);
    ASSERT_EQ(timer.isValid(), true);
    ASSERT_GE(timer.elapsed(), 77s);
    ASSERT_GE(timer.elapsedMs(), 77 * 1000);

    restartValue = timer.restart();
    ASSERT_GE(restartValue, 77s);
    ASSERT_EQ(timer.hasExpired(7s), false);
    ASSERT_EQ(timer.isValid(), true);

    timeShift.applyRelativeShift(77s);
    ASSERT_EQ(timer.hasExpired(777s), false);
    ASSERT_EQ(timer.hasExpired(7s), true);
    ASSERT_EQ(timer.isValid(), true);
    ASSERT_GE(timer.elapsed(), 77s);

    timer.invalidate();
    ASSERT_EQ(timer.hasExpired(0ms), true);
    ASSERT_EQ(timer.hasExpired(7s), true);
    ASSERT_EQ(timer.isValid(), false);
}

} // namespace nx::utils::test
