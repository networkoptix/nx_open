// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/utils/algorithm/concat.h>

namespace nx::utils::algorithm::test {

class AlgorithmConcat:
    public ::testing::Test
{
protected:
    template<typename T, std::size_t ExpectedSize, typename... C>
    constexpr void assertEqual(
        const std::array<T, ExpectedSize>& expected,
        C&&... pieces)
    {
        auto actual = concat(std::forward<C>(pieces)...);
        ASSERT_EQ(expected, actual);
    }
};

TEST_F(AlgorithmConcat, arrays)
{
    assertEqual(
        std::to_array<int>({1, 7, 5}),
        std::array<int, 1>{1}, std::array<int, 2>{7, 5}, std::array<int, 0>{});
}

} // namespace nx::utils::algorithm::test
