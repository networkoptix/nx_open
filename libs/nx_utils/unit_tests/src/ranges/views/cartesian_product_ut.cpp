// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <array>
#include <list>
#include <ranges>
#include <set>
#include <sstream>
#include <string>
#include <tuple>
#include <vector>

#include <gtest/gtest.h>

#include <nx/ranges/std_compat.h>

namespace nx::ranges::test {
namespace {

using nx::views::detail::cartesianProduct;

TEST(CartesianProduct, ProducesCompleteCombinationsInLexicographicalOrder)
{
    const std::array letters{'A', 'B'};
    const std::vector numbers{1, 2, 3};
    const std::list<std::string> symbols{"a", "b", "c", "d"};
    const std::vector<std::tuple<char, int, std::string>> kExpected{
        {'A', 1, "a"}, {'A', 1, "b"}, {'A', 1, "c"}, {'A', 1, "d"},
        {'A', 2, "a"}, {'A', 2, "b"}, {'A', 2, "c"}, {'A', 2, "d"},
        {'A', 3, "a"}, {'A', 3, "b"}, {'A', 3, "c"}, {'A', 3, "d"},
        {'B', 1, "a"}, {'B', 1, "b"}, {'B', 1, "c"}, {'B', 1, "d"},
        {'B', 2, "a"}, {'B', 2, "b"}, {'B', 2, "c"}, {'B', 2, "d"},
        {'B', 3, "a"}, {'B', 3, "b"}, {'B', 3, "c"}, {'B', 3, "d"},
    };
    const auto cartesianView = cartesianProduct(letters, numbers, symbols);

    EXPECT_EQ(
        kExpected | nx::ranges::to<std::vector>(),
        cartesianView | nx::ranges::to<std::vector>());
    EXPECT_EQ(
        kExpected | nx::ranges::to<std::set>(),
        cartesianView | nx::ranges::to<std::set>());
}

TEST(CartesianProduct, EmptyFirstAxisProducesEmptyResult)
{
    const std::array values{1, 2};
    const std::vector<std::string> empty;

    EXPECT_TRUE(cartesianProduct(empty, values).empty());
}

TEST(CartesianProduct, EmptyTailAxisProducesEmptyResult)
{
    const std::array values{1, 2};
    const std::vector<std::string> empty;

    EXPECT_TRUE(cartesianProduct(values, empty).empty());
}

TEST(CartesianProduct, FirstAxisMayBeInputRange)
{
    std::istringstream input("1 2");
    const std::array values{'a', 'b'};

    EXPECT_EQ(
        (std::vector<std::tuple<int, char>>{{1, 'a'}, {1, 'b'}, {2, 'a'}, {2, 'b'}}),
        cartesianProduct(std::ranges::istream_view<int>(input), values)
            | nx::ranges::to<std::vector>());
}

TEST(CartesianProduct, InfiniteFirstAxisCanBeBoundedDownstream)
{
    const std::array flags{false, true};
    const std::vector<std::tuple<int, bool>> kExpected{
        {1, false},
        {1, true},
        {2, false},
        {2, true},
    };
    const auto cartesianView =
        cartesianProduct(std::views::iota(1), flags) | std::views::take(kExpected.size());

    EXPECT_EQ(
        kExpected | nx::ranges::to<std::vector>(),
        cartesianView | nx::ranges::to<std::vector>());
    EXPECT_EQ(
        kExpected | nx::ranges::to<std::set>(),
        cartesianView | nx::ranges::to<std::set>());
}

struct NonDefaultConstructible
{
    NonDefaultConstructible(int value): value(value) {}
    NonDefaultConstructible(const NonDefaultConstructible&) = default;
    NonDefaultConstructible& operator=(const NonDefaultConstructible&) = delete;

    int value;
    bool operator==(const NonDefaultConstructible&) const = default;
};

TEST(CartesianProduct, DoesNotRequireDefaultConstructionOrAssignment)
{
    const std::array<NonDefaultConstructible, 2> values{1, 2};

    EXPECT_EQ(
        (std::vector<std::tuple<NonDefaultConstructible>>{{1}, {2}}),
        cartesianProduct(values) | nx::ranges::to<std::vector>());
}

TEST(CartesianProduct, IsLazyViewOfReferences)
{
    std::vector values{1, 2};
    const std::array flags{false, true};
    auto product = cartesianProduct(values, flags);

    static_assert(std::ranges::view<decltype(product)>);
    static_assert(std::same_as<
        std::ranges::range_reference_t<decltype(product)>,
        std::tuple<int&, const bool&>>);

    values[0] = 3;
    auto&& [value, flag] = *product.begin();
    value = 4;

    EXPECT_EQ(4, values[0]);
    EXPECT_EQ(false, flag);
}

TEST(CartesianProduct, SupportsEmptyProduct)
{
    EXPECT_EQ(
        (std::vector<std::tuple<>>{{}}),
        cartesianProduct() | nx::ranges::to<std::vector>());
}

} // namespace
} // namespace nx::ranges::test
