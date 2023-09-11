// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <thread>

#include <nx/utils/math/max_per_period.h>

namespace nx::utils::math::test {

class MaxPerPeriod: public ::testing::Test
{
public:
    MaxPerPeriod():
        m_timeShift(utils::test::ClockType::steady) //< Causes monotonicTime() to shift
    {
    }

protected:
    void whenAddValuesEvenlyWithinPeriod(
        std::vector<int> values,
        std::chrono::milliseconds period)
    {
        const auto timePerValue = period / (int) values.size();

        initializeMaxPerPeriod(period);

        for(std::size_t i = 0; i < values.size(); ++i)
        {
            m_timeShift.applyRelativeShift(timePerValue / 2);

            m_maxPerPeriod->add(values[i]);

            m_timeShift.applyRelativeShift(timePerValue / 2);
        }
    }

    void whenSomeTimePasses(std::chrono::milliseconds duration)
    {
        m_timeShift.applyRelativeShift(duration);
    }

    void thenMaxValueIs(int expectedValue)
    {
        ASSERT_EQ(expectedValue, m_maxPerPeriod->getMaxPerLastPeriod());
    }

private:
    void initializeMaxPerPeriod(std::chrono::milliseconds period)
    {
        m_period = period;
        m_maxPerPeriod = std::make_unique<math::MaxPerPeriod<int>>(m_period);
    }

private:
    std::chrono::milliseconds m_period;
    std::unique_ptr<nx::utils::math::MaxPerPeriod<int>> m_maxPerPeriod;
    utils::test::ScopedTimeShift m_timeShift;
};

TEST_F(MaxPerPeriod, gets_max_of_all_values_within_the_period)
{
    whenAddValuesEvenlyWithinPeriod(
        {600, 500, 400, 300, 200, 100},
        std::chrono::milliseconds(10));

    thenMaxValueIs(600);
}

TEST_F(MaxPerPeriod, gets_max_after_some_values_expire)
{
    whenAddValuesEvenlyWithinPeriod(
        {700, 600, 500, 400, 300, 200, 100},
        std::chrono::milliseconds(100));

    whenSomeTimePasses(std::chrono::milliseconds(51));

    thenMaxValueIs(300);
}

TEST_F(MaxPerPeriod, zero_max_after_all_values_expire)
{
    whenAddValuesEvenlyWithinPeriod(
        {600, 500, 400, 300, 200, 100},
        std::chrono::milliseconds(10));

    whenSomeTimePasses(std::chrono::milliseconds(11));

    thenMaxValueIs(0);
}

} // namespace nx::utils::math::test
