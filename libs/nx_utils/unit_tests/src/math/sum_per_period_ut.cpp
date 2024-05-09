// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <memory>

#include <gtest/gtest.h>

#include <nx/utils/math/sum_per_period.h>

namespace nx {
namespace utils {
namespace math {
namespace test {

class SumPerPeriod:
    public ::testing::Test
{
public:
    SumPerPeriod():
        m_period(std::chrono::minutes(1)),
        m_sumPerPeriod(m_period)
    {
    }

protected:
    std::chrono::milliseconds m_period;
    math::SumPerPeriod<int> m_sumPerPeriod;
    nx::utils::test::ScopedSyntheticMonotonicTime m_timeShift{
        // Make it easier to reason about time passing by setting intial time to 0.
        std::chrono::steady_clock::time_point{}};
    int m_value = 0;
    int m_valueCount = 0;

    std::chrono::milliseconds subperiodLength() const
    {
        using namespace std::chrono;
        return duration_cast<milliseconds>(m_sumPerPeriod.subperiodLength());
    }

    void setSubPeriodCount(int count)
    {
        std::destroy_at(&m_sumPerPeriod);
        new (&m_sumPerPeriod) math::SumPerPeriod<int>(m_period, count);
    }

    void addMultipleValuesWithinPeriod(std::chrono::milliseconds period)
    {
        m_value = 1000;
        m_valueCount = 7;
        for (int i = 0; i < 7; ++i)
            m_sumPerPeriod.add(m_value, period);
    }

    void assertSumEqualTo(int value)
    {
        ASSERT_LE(
            m_sumPerPeriod.getSumPerLastPeriod(),
            value * (1 + m_sumPerPeriod.maxError()));
        ASSERT_GE(
            m_sumPerPeriod.getSumPerLastPeriod(),
            value * (1 - m_sumPerPeriod.maxError()));
    }
};

TEST_F(SumPerPeriod, single_value_fits_in_period)
{
    const auto value = 1000;
    m_sumPerPeriod.add(value, m_period / 2);
    ASSERT_EQ(value, m_sumPerPeriod.getSumPerLastPeriod());
}

TEST_F(SumPerPeriod, single_value_greater_than_period)
{
    const auto value = 1000;

    m_sumPerPeriod.add(value, 2 * m_period);
    assertSumEqualTo(value / 2);
}

TEST_F(SumPerPeriod, single_value_greater_than_period_2)
{
    const auto value = 1000;

    m_sumPerPeriod.add(value, 2 * m_period);
    assertSumEqualTo(value / 2);
}

TEST_F(SumPerPeriod, two_values_added_over_double_the_period_equals_the_value)
{
    const auto value = 1000;

    m_sumPerPeriod.add(value, 2 * m_period);
    m_sumPerPeriod.add(value, 2 * m_period);

    assertSumEqualTo(value);
}

TEST_F(SumPerPeriod, two_values_greater_than_period_with_expiration)
{
    const auto value = 1000;

    m_sumPerPeriod.add(value, 2 * m_period);
    m_sumPerPeriod.add(value, 2 * m_period);

    m_timeShift.applyRelativeShift(m_period / 2);

    assertSumEqualTo(value / 2);
}

TEST_F(SumPerPeriod, m_front_and_m_back_work_correctly)
{
    const auto value = 1000;

    m_sumPerPeriod.add(value, 2 * m_period);  // 500

    m_timeShift.applyRelativeShift(m_period); // -500

    m_sumPerPeriod.add(value, 2 * m_period);  // 500

    assertSumEqualTo(500);
}

TEST_F(SumPerPeriod, half_value_added_over_half_the_period_twice)
{
    const auto value = 1000;

    m_sumPerPeriod.add(value / 2, m_period / 2); // 500
    m_sumPerPeriod.add(value / 2, m_period / 2); // 500
    m_sumPerPeriod.add(value / 2, m_period / 2); // 500
    m_sumPerPeriod.add(value / 2, m_period / 2); // 500

    assertSumEqualTo(value * 2);
}

TEST_F(SumPerPeriod, multiple_values_fit_in_period)
{
    addMultipleValuesWithinPeriod(m_period / 2);
    assertSumEqualTo(m_value * m_valueCount);
}

TEST_F(SumPerPeriod, value_expires_completely)
{
    addMultipleValuesWithinPeriod(m_period / 2);
    m_timeShift.applyRelativeShift(m_period + std::chrono::milliseconds(1));
    assertSumEqualTo(0);
}

TEST_F(SumPerPeriod, value_expires_partially)
{
    addMultipleValuesWithinPeriod(m_period);
    m_timeShift.applyRelativeShift(m_period / 2);
    assertSumEqualTo(m_value * m_valueCount / 2);
}

TEST_F(SumPerPeriod, value_present_on_period_boundary)
{
    m_sumPerPeriod.add(1000);
    m_timeShift.applyRelativeShift(m_period);
    assertSumEqualTo(1000);
}

TEST_F(SumPerPeriod, value_expires_after_period)
{
    m_sumPerPeriod.add(1000);
    // Counterintuitive but the newest subperiod expires in the future by one subperiod.
    m_timeShift.applyRelativeShift(m_period + subperiodLength());
    assertSumEqualTo(0);
}

TEST_F(SumPerPeriod, recovers_after_not_being_used_for_a_long_time)
{
    m_sumPerPeriod.add(1000);

    // m_period * 2 + subperiodLenght() is the smallest amount of time that must pass
    // before it is impossible to recover SumPerPeriod::m_currentPeriodStart without resetting
    // its value.
    m_timeShift.applyRelativeShift(m_period * 2 + subperiodLength());

    m_sumPerPeriod.add(1000);
    assertSumEqualTo(1000);
}

TEST_F(SumPerPeriod, reset)
{
    for (int i = 0; i < 10; ++i)
    {
        m_sumPerPeriod.add(1000, m_period * 2);
        m_sumPerPeriod.add(1000, m_period * 2);
        assertSumEqualTo(1000);
        m_sumPerPeriod.reset();
    }
}

} // namespace test
} // namespace math
} // namespace utils
} // namespace
