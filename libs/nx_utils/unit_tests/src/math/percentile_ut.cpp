// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <algorithm>
#include <gtest/gtest.h>

#include <nx/utils/random.h>
#include <nx/utils/math/percentile.h>

namespace nx::utils::math::test {

class Percentile: public ::testing::Test
{
public:
    template<typename ValueType>
    std::vector<ValueType> randomNumbers(int count)
    {
        std::vector<ValueType> result;

        for (int i = 0; i < count; ++i)
            result.emplace_back(nx::utils::random::number<ValueType>());

        return result;
    }

    template<typename ValueType>
    void addNumbersAndAssertPercentileReturnsExpected(
        double percentile,
        math::Percentile<ValueType> * p,
        const std::vector<ValueType>& input)
    {
        std::vector<ValueType> sorted;
        sorted.reserve(input.size());

        for (const auto& i: input)
        {
            p->add(i);

            sorted.emplace_back(i);
            std::sort(sorted.begin(), sorted.end());

            ASSERT_EQ(sorted[sorted.size() * percentile], p->get());
        }
    }

};

TEST_F(Percentile, p50)
{
    const auto input = randomNumbers<int>(15);

    nx::utils::math::Percentile<int>p(0.5);

    addNumbersAndAssertPercentileReturnsExpected(0.5, &p, input);
}

TEST_F(Percentile, p30_contrived_example)
{
    const std::vector<int> input {5, 9, 7, 12, 3, 11, 20, 4};

    nx::utils::math::Percentile<int> p(0.3);

    addNumbersAndAssertPercentileReturnsExpected(0.3, &p, input);
}

TEST_F(Percentile, p75_input_increasing)
{
    auto input = randomNumbers<double>(20);
    std::sort(input.begin(), input.end());

    nx::utils::math::Percentile<double> p(0.75);

    addNumbersAndAssertPercentileReturnsExpected(0.75, &p, input);
}

TEST_F(Percentile, p75_input_decreasing)
{
    auto input = randomNumbers<int>(19);
    std::sort(input.begin(), input.end(), std::greater<int>{});

    nx::utils::math::Percentile<int> p(0.75);

    addNumbersAndAssertPercentileReturnsExpected(0.75, &p, input);
}

TEST_F(Percentile, p99)
{
    const auto scalar = nx::utils::random::number(2, 4);
    auto input = randomNumbers<double>(99 * scalar);

    nx::utils::math::Percentile<double> p(0.99);

    addNumbersAndAssertPercentileReturnsExpected(0.99, &p, input);
}

} // namespace nx::utils::math::test
