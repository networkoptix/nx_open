// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <deque>
#include <vector>

#include <gtest/gtest.h>

#include <nx/ranges/actions/partition_sums.h>
#include <nx/ranges/std_compat.h>

namespace nx::actions::test {
namespace {

struct Car
{
    std::string mark;
    bool operator==(const Car&) const = default;
};

struct Person
{
    std::string name;
    bool operator==(const Person&) const = default;
};

struct Error
{
    std::string what;
    bool operator==(const Error&) const = default;
};

// GTest printer
[[maybe_unused]] void PrintTo(const Car& v, std::ostream* os)
{
    *os << "Car{mark=\"" << v.mark << "\"}";
}

// GTest printer
[[maybe_unused]] void PrintTo(const Person& v, std::ostream* os)
{
    *os << "Person{name=\"" << v.name << "\"}";
}

// GTest printer
[[maybe_unused]] void PrintTo(const Error& v, std::ostream* os)
{
    *os << "Error{what=\"" << v.what << "\"}";
}

template<typename... Args>
concept CanCallPartitionSums = requires { nx::actions::partitionSums(std::declval<Args>()...); };

template<typename Lhs>
concept CanPipePartitionSums = requires { std::declval<Lhs>() | nx::actions::partitionSums; };

// not a range / wrong arity
static_assert(!CanCallPartitionSums<int>);
static_assert(!CanCallPartitionSums<int, int>);
static_assert(!CanPipePartitionSums<int>);

// unsupported element type
static_assert(!CanCallPartitionSums<std::vector<int>&>);
static_assert(!CanPipePartitionSums<std::vector<int>&>);
static_assert(!CanCallPartitionSums<std::vector<std::optional<int>>&>);
static_assert(!CanPipePartitionSums<std::vector<std::pair<int, int>>&>);

TEST(ranges, partition_sums_expected_from_strings)
{
    const std::vector<std::string> input{
        "4",
        "8",
        "oceanic",
        "15",
        "16",
        "23",
        "42",
        "lost",
    };

    const auto identify = [](const std::string& s) -> std::expected<int, std::string>
    {
        int v{};
        const auto [p, ec] = std::from_chars(s.data(), s.data() + s.size(), v);
        if (ec == std::errc{} && p == s.data() + s.size())
            return v;

        return std::unexpected(s);
    };

    // baseline
    {
        const auto [values, errors] =
            input | std::views::transform(identify) | nx::actions::partitionSums;

        ASSERT_EQ(values, (std::vector{4, 8, 15, 16, 23, 42}));
        ASSERT_EQ(errors, (std::vector<std::string>{"oceanic", "lost"}));
    }

    // drop before partitioning
    {
        const auto [values, errors] = input
            | std::views::drop(1)
            | std::views::transform(identify)
            | nx::actions::partitionSums;

        ASSERT_EQ(values, (std::vector{8, 15, 16, 23, 42}));
        ASSERT_EQ(errors, (std::vector<std::string>{"oceanic", "lost"}));
    }

    // take_n before partitioning
    {
        const auto [values, errors] = input
            | std::views::take(4)
            | std::views::transform(identify)
            | nx::actions::partitionSums;

        ASSERT_EQ(values, (std::vector{4, 8, 15}));
        ASSERT_EQ(errors, (std::vector<std::string>{"oceanic"}));
    }

    // take_while(kMaxErrors) before partitioning
    {
        constexpr std::size_t kMaxErrors = 1;
        std::size_t errorsSeen = 0;

        // TODO: #skolesnik std::views::take_while does not support/allow mutable predicate since
        // it doesn't guarantee a single predicate evaluation for an element (even for single
        // pass).
        const std::function takeUpToNErrors = [&](const std::expected<int, std::string>& e) -> bool
        {
            if (errorsSeen >= kMaxErrors)
                return false;

            if (!e)
                ++errorsSeen;

            return true;
        };

        const auto [values, errors] = input
            | std::views::transform(identify)
            | std::views::take_while(takeUpToNErrors)
            | nx::actions::partitionSums;

        ASSERT_EQ(values, (std::vector{4, 8}));
        ASSERT_EQ(errors, (std::vector<std::string>{"oceanic"}));
    }

    // filtered view before partitioning: keep only expected.has_value()
    {
        const auto [values, errors] = input
            | std::views::transform(identify)
            | std::views::filter(
                [](const std::expected<int, std::string>& e) -> bool
                {
                    return e.has_value();
                })
            | nx::actions::partitionSums;

        ASSERT_EQ(values, (std::vector<int>{4, 8, 15, 16, 23, 42}));
        ASSERT_TRUE(errors.empty());
    }
}

TEST(ranges, partition_sums_variant_from_strings)
{
    const std::vector<std::string> input{
        "4",
        "8",
        "oceanic",
        "15",
        "16",
        "stop",
        "23",
    };

    const auto identify =
        [](const std::string& s) -> std::variant<int, std::string>
        {
            int v{};
            const auto [p, ec] = std::from_chars(s.data(), s.data() + s.size(), v);
            if (ec == std::errc{} && p == s.data() + s.size())
                return v;

            return s;
        };

    // baseline
    {
        const auto [ints, strs] =
            input | std::views::transform(identify) | nx::actions::partitionSums;

        ASSERT_EQ(ints, (std::vector{4, 8, 15, 16, 23}));
        ASSERT_EQ(strs, (std::vector<std::string>{"oceanic", "stop"}));
    }

    // drop before partitioning
    {
        const auto [ints, strs] = input
            | std::views::drop(2)
            | std::views::transform(identify)
            | nx::actions::partitionSums;

        ASSERT_EQ(ints, (std::vector{15, 16, 23}));
        ASSERT_EQ(strs, (std::vector<std::string>{"oceanic", "stop"}));
    }

    // take_n before partitioning
    {
        const auto [ints, strs] = input
            | std::views::take(3)
            | std::views::transform(identify)
            | nx::actions::partitionSums;

        ASSERT_EQ(ints, (std::vector{4, 8}));
        ASSERT_EQ(strs, (std::vector<std::string>{"oceanic"}));
    }

    // take_while before partitioning (stop token)
    {
        const auto [ints, strs] = input
            | std::views::take_while(
                [](const std::string& s) -> bool
                {
                    return s != "stop";
                })
            | std::views::transform(identify)
            | nx::actions::partitionSums;

        ASSERT_EQ(ints, (std::vector{4, 8, 15, 16}));
        ASSERT_EQ(strs, (std::vector<std::string>{"oceanic"}));
    }
}

TEST(ranges, partition_sums_variant_custom_sum_types)
{
    const std::vector<std::string> input{
        "car_1",
        "woman_alice",
        "??",
        "man_bob",
        "car_2",
        "man_JohnCena",
        "x",
        "bell_429",
        "car_3",
    };

    const auto identify = [](const std::string& s) -> std::variant<Car, Person, Error>
    {
        const std::string_view sv{s};

        if (sv.starts_with("car_"))
            return Car{std::string(sv.substr(4))};

        if (sv.starts_with("man_"))
            return Person{std::string(sv.substr(4))};

        if (sv.starts_with("woman_"))
            return Person{std::string(sv.substr(6))};

        return Error{s};
    };

    // baseline
    {
        const auto [cars, people, errors] =
            input
            | std::views::transform(identify)
            | nx::actions::partitionSums;

        ASSERT_EQ(cars, (std::vector<Car>{{"1"}, {"2"}, {"3"}}));
        ASSERT_EQ(people, (std::vector<Person>{{"alice"}, {"bob"}, {"JohnCena"}}));
        ASSERT_EQ(errors, (std::vector<Error>{{"??"}, {"x"}, {"bell_429"}}));
    }

    {
        const auto [cars, people, errors] = input
            | std::views::drop(1)
            | std::views::transform(identify)
            | nx::actions::partitionSums;

        ASSERT_EQ(cars, (std::vector<Car>{{"2"}, {"3"}}));
        ASSERT_EQ(people, (std::vector<Person>{{"alice"}, {"bob"}, {"JohnCena"}}));
        ASSERT_EQ(errors, (std::vector<Error>{{"??"}, {"x"}, {"bell_429"}}));
    }

    {
        const auto [cars, people, errors] = input
            | std::views::take(4)
            | std::views::transform(identify)
            | nx::actions::partitionSums;

        ASSERT_EQ(cars, (std::vector<Car>{{"1"}}));
        ASSERT_EQ(people, (std::vector<Person>{{"alice"}, {"bob"}}));
        ASSERT_EQ(errors, (std::vector<Error>{{"??"}}));
    }

    {
        constexpr std::size_t kMaxErrors = 1;
        std::size_t errorsSeen = 0;

        // TODO: #skolesnik std::views::take_while does not support/allow mutable predicate since
        // it doesn't guarantee a single predicate evaluation for an element (even for single
        // pass).
        const std::function takeUpToNErrors =
            [&](const std::variant<Car, Person, Error>& v) -> bool
            {
                if (errorsSeen >= kMaxErrors)
                    return false;

                if (std::holds_alternative<Error>(v))
                    ++errorsSeen;

                return true;
            };

        const auto [cars, people, errors] = input
            | std::views::transform(identify)
            | std::views::take_while(takeUpToNErrors)
            | nx::actions::partitionSums;

        ASSERT_EQ(cars, (std::vector<Car>{{"1"}}));
        ASSERT_EQ(people, (std::vector<Person>{{"alice"}}));
        ASSERT_EQ(errors, (std::vector<Error>{{"??"}}));
    }

    {
        const auto [cars, people, errors] =
            input
            | std::views::transform(identify)
            | std::views::filter(
                [](const std::variant<Car, Person, Error>& v) -> bool
                {
                    return std::holds_alternative<Person>(v);
                })
            | nx::actions::partitionSums;

        ASSERT_TRUE(cars.empty());
        ASSERT_EQ(people, (std::vector<Person>{{"alice"}, {"bob"}, {"JohnCena"}}));
        ASSERT_TRUE(errors.empty());
    }

    {
        const auto [cars, people, errors] =
            input
            | std::views::transform(identify)
            | std::views::filter(
                [](const std::variant<Car, Person, Error>& v) -> bool
                {
                    return std::holds_alternative<Person>(v);
                })
            | std::views::filter(
                [](const std::variant<Car, Person, Error>& v) -> bool
                {
                    return std::get<Person>(v).name != "JohnCena";
                })
            | nx::actions::partitionSums;

        ASSERT_TRUE(cars.empty());
        ASSERT_EQ(people, (std::vector<Person>{{"alice"}, {"bob"}}));
        ASSERT_TRUE(errors.empty());
    }
}

} // namespace
} // namespace nx::actions::test
