// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <set>
#include <span>
#include <vector>

#include <gtest/gtest.h>

#include <nx/ranges/actions/unique.h>
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

// ad hoc for static_assert check
template<typename T>
concept UniqueInvocable = requires { nx::actions::unique(std::declval<T>()); };

static_assert(!UniqueInvocable<std::vector<int>&>);
static_assert(!UniqueInvocable<std::set<int>>);

TEST(ranges, unique_moved_container)
{
    std::vector input{1, 1, 2, 2, 2, 3, 3};
    const std::vector out = nx::actions::unique(std::move(input));
    EXPECT_EQ(out, (std::vector{1, 2, 3}));
}

TEST(ranges, unique_temporary_pipe)
{
    const std::vector out = std::vector{7, 7, 7, 8, 8, 9} | nx::actions::unique();
    EXPECT_EQ(out, (std::vector{7, 8, 9}));
}

TEST(ranges, unique_const_copy_then_move)
{
    const auto input = std::vector{1, 1, 2, 2, 3, 3};

    // explicit copy
    const std::vector out = nx::actions::unique(std::vector(input));
    EXPECT_EQ(out, (std::vector{1, 2, 3}));
}

TEST(ranges, unique_filtered_view_input)
{
    const std::vector out =
        nx::actions::unique(
            std::vector{1, 2, 2, 3, 4, 4, 5, 6, 6}
            | std::views::filter(
                [](int x)
                {
                    return (x % 2) == 0;
                }))
        | nx::ranges::to<std::vector>();

    EXPECT_EQ(out, (std::vector{2, 4, 6}));
}

TEST(ranges, unique_span_noerase_subrange)
{
    std::vector storage{0, 1, 1, 2, 2, 2, 3, 3, 9};
    const std::vector out = nx::actions::unique(std::span{storage.data() + 1, storage.size() - 2})
        | nx::ranges::to<std::vector>();

    EXPECT_EQ(out, (std::vector{1, 2, 3}));
}

TEST(ranges, unique_moveonly_erasable_projection)
{
    std::vector input =
        nx::make_vector<MoveOnly>(MoveOnly{1}, MoveOnly{1}, MoveOnly{2}, MoveOnly{2}, MoveOnly{3});

    const std::vector result = nx::actions::unique(std::move(input), {}, &MoveOnly::key)
        | std::views::transform(&MoveOnly::key)
        | nx::ranges::to<std::vector>();

    EXPECT_EQ(result, (std::vector{1, 2, 3}));
}

TEST(ranges, unique_moveonly_span_noerase_projection)
{
    std::vector storage =
        nx::make_vector<MoveOnly>(MoveOnly{1}, MoveOnly{1}, MoveOnly{2}, MoveOnly{2}, MoveOnly{3});

    const std::vector keys =
        nx::actions::unique(std::span{storage.data(), storage.size()}, {}, &MoveOnly::key)
        | std::views::transform(&MoveOnly::key)
        | nx::ranges::to<std::vector>();

    EXPECT_EQ(keys, (std::vector{1, 2, 3}));
}

TEST(ranges, unique_pipeline_by_name_to_vector)
{
    using namespace std::string_literals;

    const std::vector result =
        std::vector<Person>{
            {.name = "Ana", .age = 10},
            {.name = "Ana", .age = 16},
            {.name = "Bob", .age = 12},
            {.name = "Bob", .age = 99},
            {.name = "Cora", .age = 9},
            {.name = "Cora", .age = 14},
        }
        | nx::actions::unique({}, &Person::name)
        | std::views::transform(&Person::name)
        | nx::ranges::to<std::vector>();

    EXPECT_EQ(result, (std::vector{"Ana"s, "Bob"s, "Cora"s}));
}

} // namespace
} // namespace nx::actions::test
