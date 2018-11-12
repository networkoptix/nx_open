#include <algorithm>
#include <deque>
#include <functional>

#include <gtest/gtest.h>

#include <nx/utils/statistics/top_value_per_period_calculator.h>

namespace nx {
namespace utils {
namespace statistics {
namespace test {

static constexpr std::chrono::milliseconds kCalculationPeriod = std::chrono::hours(1);

class TopValuePerPeriodCalculator:
    public ::testing::Test
{
public:
    TopValuePerPeriodCalculator():
        m_calculator(kCalculationPeriod),
        m_timeShift(utils::test::ClockType::steady)
    {
    }

protected:
    void addValue(int value)
    {
        m_values.push_back(value);
        m_calculator.add(value);
    }

    void addMultipleValues()
    {
        for (int i = 0; i < 11; ++i)
            addValue(rand());
    }

    void waitForEveryValueToExpire()
    {
        m_timeShift.applyRelativeShift(kCalculationPeriod * 2);
        m_values.clear();
    }

    void wait(std::chrono::milliseconds delay)
    {
        m_timeShift.applyRelativeShift(delay);
    }

    void assertMaxIsReported()
    {
        ASSERT_EQ(
            *std::max_element(m_values.begin(), m_values.end()),
            m_calculator.top());
    }

    void assertTopValueIs(int expected)
    {
        ASSERT_EQ(expected, m_calculator.top());
    }

private:
    statistics::TopValuePerPeriodCalculator<int, std::greater<int>> m_calculator;
    std::deque<int> m_values;
    utils::test::ScopedTimeShift m_timeShift;
};

TEST_F(TopValuePerPeriodCalculator, correct_value_is_provided)
{
    addMultipleValues();
    assertMaxIsReported();
}

TEST_F(TopValuePerPeriodCalculator, correct_value_is_provided_after_some_values_expire)
{
    addMultipleValues();
    waitForEveryValueToExpire();

    addMultipleValues();
    assertMaxIsReported();
}

TEST_F(TopValuePerPeriodCalculator, maximum_value_expire)
{
    addValue(100);
    wait(kCalculationPeriod / 2);

    addValue(90);
    wait(kCalculationPeriod / 4 * 3);

    assertTopValueIs(90);
}

} // namespace test
} // namespace statistics
} // namespace utils
} // namespace nx
