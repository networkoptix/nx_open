// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <array>
#include <iterator>
#include <map>
#include <ranges>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include <nx/ranges/to.h>

namespace nx::ranges::test {
namespace {

using nx::ranges::detail::to;

struct Entry
{
    int first;
    int second;

    bool operator==(const Entry&) const = default;
};

TEST(ToFallback, DeducesSequenceContainer)
{
    const std::array values{1, 2};

    EXPECT_EQ((std::vector{1, 2}), values | to<std::vector>());
}

TEST(ToFallback, CanBeInvokedDirectly)
{
    const std::array values{1, 2};

    EXPECT_EQ((std::vector{1, 2}), to<std::vector>()(values));
}

TEST(ToFallback, DeducesAssociativeContainer)
{
    const std::array<std::pair<int, std::string>, 2> values{{{1, "one"}, {2, "two"}}};

    EXPECT_EQ(
        (std::map<int, std::string>{{1, "one"}, {2, "two"}}),
        values | to<std::map>());
}

TEST(ToFallback, MaterializesCommonableSentinelRange)
{
    const std::array<Entry, 2> values{{{1, 10}, {2, 20}}};
    const auto range = std::ranges::subrange(
        std::counted_iterator(values.begin(), std::ssize(values)),
        std::default_sentinel);

    static_assert(!std::ranges::common_range<decltype(range)>);
    EXPECT_EQ((std::vector<Entry>{{1, 10}, {2, 20}}), range | to<std::vector>());
}

TEST(ToFallback, MaterializesSinglePassPairLikeRange)
{
    std::istringstream input("1 2");

    EXPECT_EQ(
        (std::vector<Entry>{{1, 10}, {2, 20}}),
        std::ranges::istream_view<int>(input)
            | std::views::transform(
                [](int value) -> Entry { return {.first = value, .second = 10 * value}; })
            | to<std::vector>());
}

TEST(ToFallback, MaterializesSinglePassRangeIntoAssociativeContainer)
{
    std::istringstream input("1 2");

    EXPECT_EQ(
        (std::map<int, int>{{1, 10}, {2, 20}}),
        std::ranges::istream_view<int>(input)
            | std::views::transform(
                [](int value) { return std::pair{value, 10 * value}; })
            | to<std::map>());
}

TEST(ToFallback, MaterializesConcreteString)
{
    EXPECT_EQ("value", std::string_view("value") | to<std::string>());
}

TEST(ToFallback, MaterializesSinglePassRangeIntoConcreteContainer)
{
    std::istringstream input("1 2");

    EXPECT_EQ(
        (std::vector{1, 2}),
        std::ranges::istream_view<int>(input) | to<std::vector<int>>());
}

} // namespace
} // namespace nx::ranges::test
