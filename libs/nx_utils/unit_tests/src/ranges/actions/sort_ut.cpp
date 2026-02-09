// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <set>
#include <span>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include <nx/ranges/actions/sort.h>
#include <nx/ranges/std_compat.h>
#include <nx/utils/std/cppnx.h>

namespace nx::actions::test {
namespace {

struct MoveOnly
{
    int key = 0;

    explicit MoveOnly(int k):
        key(k)
    {
    }

    MoveOnly(MoveOnly&&) noexcept = default;
    MoveOnly& operator=(MoveOnly&&) noexcept = default;
    MoveOnly(const MoveOnly&) = delete;
    MoveOnly& operator=(const MoveOnly&) = delete;
};

struct Person
{
    std::string name;
    int age;
};

// ad hoc for static_assert
template<typename T>
concept SortInvocable = requires { nx::actions::sort(std::declval<T>()); };

static_assert(!SortInvocable<std::vector<int>&>);
static_assert(!SortInvocable<std::set<int>>);

TEST(ranges, sort_moved_container)
{
    std::vector input{3, 1, 2, 2, 5, 4};
    const std::vector out = nx::actions::sort(std::move(input));
    EXPECT_EQ(out, (std::vector{1, 2, 2, 3, 4, 5}));
}

TEST(ranges, sort_temporary_pipe)
{
    const std::vector out = std::vector{9, 7, 8, 7} | nx::actions::sort();
    EXPECT_EQ(out, (std::vector{7, 7, 8, 9}));
}

TEST(ranges, sort_const_copy_then_move)
{
    const auto input = std::vector{3, 2, 1, 1};

    const std::vector out = nx::actions::sort(std::vector(input));
    EXPECT_EQ(out, (std::vector{1, 1, 2, 3}));
}

TEST(ranges, sort_span)
{
    std::vector storage{3, 1, 2, 2, 4};
    const std::vector out = nx::actions::sort(std::span{storage.data(), storage.size()})
        | nx::ranges::to<std::vector>();

    EXPECT_EQ(out, (std::vector{1, 2, 2, 3, 4}));
}

TEST(ranges, sort_moveonly_projection)
{
    std::vector input =
        nx::make_vector<MoveOnly>(MoveOnly{3}, MoveOnly{1}, MoveOnly{2}, MoveOnly{2});

    const std::vector keys = nx::actions::sort(std::move(input), {}, &MoveOnly::key)
        | std::views::transform(&MoveOnly::key)
        | nx::ranges::to<std::vector>();

    EXPECT_EQ(keys, (std::vector{1, 2, 2, 3}));
}

TEST(ranges, sort_pipeline_by_name_to_vector)
{
    using namespace std::string_literals;

    const std::vector result =
        std::vector<Person>{
            {.name = "Cora", .age = 14},
            {.name = "Ana", .age = 16},
            {.name = "Bob", .age = 12},
            {.name = "Ana", .age = 10},
            {.name = "Bob", .age = 99},
            {.name = "Cora", .age = 9},
        }
        | nx::actions::sort({}, &Person::name) | std::views::transform(&Person::name)
        | nx::ranges::to<std::vector>();

    EXPECT_EQ(result, (std::vector{"Ana"s, "Ana"s, "Bob"s, "Bob"s, "Cora"s, "Cora"s}));
}

} // namespace
} // namespace nx::actions::test
