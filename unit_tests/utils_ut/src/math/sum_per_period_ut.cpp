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
        m_sumPerPeriod(m_period),
        m_timeShift(utils::test::ClockType::steady)
    {
    }

protected:
    std::chrono::milliseconds m_period;
    math::SumPerPeriod<int> m_sumPerPeriod;
    utils::test::ScopedTimeShift m_timeShift;
    int m_value = 0;
    int m_valueCount = 0;

    void addMultipleValuesWithinPeriod(std::chrono::milliseconds period)
    {
        m_value = 10;
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
    const auto value = 10;
    m_sumPerPeriod.add(value, m_period / 2);
    ASSERT_EQ(value, m_sumPerPeriod.getSumPerLastPeriod());
}

TEST_F(SumPerPeriod, single_value_greater_than_period)
{
    const auto value = 10;
    m_sumPerPeriod.add(value, 2 * m_period);
    assertSumEqualTo(value / 2);
}

TEST_F(SumPerPeriod, multiple_values_fit_in_period)
{
    addMultipleValuesWithinPeriod(m_period / 2);
    assertSumEqualTo(m_value * m_valueCount);
}

TEST_F(SumPerPeriod, value_expires_completely)
{
    addMultipleValuesWithinPeriod(m_period / 2);
    m_timeShift.applyRelativeShift(m_period * 2);
    assertSumEqualTo(0);
}

TEST_F(SumPerPeriod, value_expires_partially)
{
    addMultipleValuesWithinPeriod(m_period);
    m_timeShift.applyRelativeShift(m_period / 2);
    assertSumEqualTo(m_value * m_valueCount / 2);
}

} // namespace test
} // namespace math
} // namespace utils
} // namespace nx
