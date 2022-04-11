// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/utils/math/percentile_per_period.h>

namespace nx::utils::math::test {

class PercentilePerPeriod: public ::testing::Test
{
public:
    PercentilePerPeriod()
        :
        m_time(std::chrono::steady_clock::time_point())
        {
        }

    template<typename ValueType>
    void givenValues(math::PercentilePerPeriod<ValueType> * p, ValueType start, ValueType end)
    {
        while (start < end)
            p->add(start++);

        while (start > end)
            p->add(start--);
    }

    void whenTimePasses(std::chrono::milliseconds amountOfTime)
    {
        m_time.applyRelativeShift(amountOfTime);
    }

private:
    nx::utils::test::ScopedSyntheticMonotonicTime m_time;
};

TEST_F(PercentilePerPeriod, within_first_collection_period)
{
    math::PercentilePerPeriod<int> p(0.5, std::chrono::seconds(60), 2);

    givenValues(&p, 0, 100);

    // since not enough time has passed for first period to close, no data should be reported.
    ASSERT_EQ(0, p.get());
}

TEST_F(PercentilePerPeriod, within_reporting_period)
{
    math::PercentilePerPeriod<long> p(0.5, std::chrono::seconds(60));

    givenValues(&p, 0L, 100L);

    whenTimePasses(std::chrono::seconds(60));

    ASSERT_EQ(50, p.get());
}

TEST_F(PercentilePerPeriod, reporting_period_2_percentiles)
{
    math::PercentilePerPeriod<double> p(0.5, std::chrono::seconds(60), 2 /*percentileCount*/);

    givenValues(&p, 0.0, 100.0);
    // Time 0s
    // p1: 0 - 99
    // p2: -

    whenTimePasses(std::chrono::seconds(30));
    // Time 30s

    givenValues(&p, 100.0, 200.0);
    // p1: 0 - 199
    // p2: 100 - 199

    whenTimePasses(std::chrono::seconds(30));
    // Time 60s
    // p1: -, reports 100
    // p2: 100 - 199

    ASSERT_EQ(100, p.get());

    whenTimePasses(std::chrono::seconds(30));
    // Time 90s
    // p1: -
    // p2: -, reports 150

    ASSERT_EQ(150, p.get());

    givenValues(&p, 200.0, 300.0);
    // p1: 200 - 299.
    // p2: 200 - 299, reports 150

    ASSERT_EQ(150, p.get());
}

TEST_F(PercentilePerPeriod, percentile_3_count)
{
    /**
     * p#: internal percentile implementation: e.g. p1
     * C: collection period for a given percentile
     * R: report period for a given percentile
     * 0          20         40         60         80         100        120
     * |----------|----------|----------|----------|----------|----------|----------
     * | p1C                            | p1R      |
     * |          | p2C                            | p2R      |
     * |                     | p3C                            | p3R      |
     * |                                | p1C                            | p1R      |
     */

    const auto period = std::chrono::seconds(60);
    const auto percentileCount = 3;
    const auto overlap = period / percentileCount;

    math::PercentilePerPeriod<int> p(0.5, period, percentileCount); //< Time 0s

    givenValues(&p, 0, 100);

    whenTimePasses(overlap); //< Time 20s

    givenValues(&p, 100, 200);

    whenTimePasses(overlap); // Time 40s

    givenValues(&p, 200, 300);

    whenTimePasses(overlap); // Time 60s

    ASSERT_EQ(150, p.get());

    whenTimePasses(overlap); // Time 80s

    ASSERT_EQ(200, p.get());

    whenTimePasses(overlap); // Time 100s

    ASSERT_EQ(250, p.get());

    whenTimePasses(overlap); // Time 120s

    ASSERT_EQ(0, p.get());
}

TEST_F(PercentilePerPeriod, resets_after_no_data_through_latest_collection_period)
{
    math::PercentilePerPeriod<std::chrono::milliseconds> p(
        0.5,
        std::chrono::seconds(60),
        2 /*percentileCount*/);

    givenValues(&p, std::chrono::milliseconds(0), std::chrono::milliseconds(100));

    whenTimePasses(std::chrono::seconds(60));

    ASSERT_EQ(std::chrono::milliseconds(50), p.get());

    // it must be later in time than the latest percentile collection period.
    whenTimePasses(std::chrono::seconds(90));

    ASSERT_EQ(std::chrono::seconds(0), p.get());
}

} // namespace nx::utils::math::test
