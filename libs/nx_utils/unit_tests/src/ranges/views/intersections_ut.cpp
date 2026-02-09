// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <deque>
#include <set>
#include <unordered_set>
#include <vector>

#include <gtest/gtest.h>

#include <nx/ranges.h>
#include <nx/ranges/views/intersections.h>
#include <nx/utils/std/cppnx.h>

namespace nx::utils::test {
namespace {

using namespace std::chrono_literals;

struct NotConvertible
{
};

struct CopyOnly {
    CopyOnly() = default;
    CopyOnly(CopyOnly const&) = default;
    CopyOnly(CopyOnly&&) = delete;
    CopyOnly& operator=(CopyOnly const&) = default;
    CopyOnly& operator=(CopyOnly&&) = delete;
};

struct MoveOnly {
    MoveOnly() = default;
    MoveOnly(MoveOnly const&) = delete;
    MoveOnly(MoveOnly&&) = default;
    MoveOnly& operator=(MoveOnly const&) = delete;
    MoveOnly& operator=(MoveOnly&&) = default;
};

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

TEST(ranges, append)
{
    // temporaries: chaining
    {
        const std::vector out =
            nx::make_vector(1)
            | nx::actions::append(4)
            | nx::actions::append(815);

        ASSERT_EQ(out, (std::vector{1, 4, 815}));
    }

    // direct values
    {
        std::vector vec{1, 2, 3};
        const std::vector out = nx::actions::append(std::move(vec), 4, 8, 15);
        ASSERT_EQ(out, (std::vector{1, 2, 3, 4, 8, 15}));
    }

    // moved object pipe (deque)
    {
        std::deque deq{10};
        const std::deque out = std::move(deq) | nx::actions::append(20, 30);
        ASSERT_EQ(out, (std::deque{10, 20, 30}));
    }

    // associative: unordered_set insert path
    {
        std::unordered_set<int> uSet;
        const std::unordered_set out = std::move(uSet) | nx::actions::append(7, 7, 8);
        ASSERT_EQ(out.size(), 2u);
        ASSERT_TRUE(out.contains(7));
        ASSERT_TRUE(out.contains(8));
    }

    // set insert + duplicates
    {
        std::set set{2};
        const std::set out = std::move(set) | nx::actions::append(1, 2, 3, 3);
        ASSERT_EQ(out, (std::set{1, 2, 3}));
    }

    // append a range element to a container-of-ranges
    {
        const std::vector out = std::vector<std::vector<int>>{}
            | nx::actions::append(std::vector{1, 2})
            | nx::actions::append(std::vector{3});

        ASSERT_EQ(out.size(), 2u);
        ASSERT_EQ(out[0], (std::vector{1, 2}));
        ASSERT_EQ(out[1], (std::vector{3}));
    }

    // move-only elements via rvalue closure
    {
        const auto make = [] { return std::vector<std::unique_ptr<int>>{}; };
        const std::vector out = make()
            | nx::actions::append(std::make_unique<int>(1))
            | nx::actions::append(std::make_unique<int>(2));

        ASSERT_EQ(out.size(), 2u);
        ASSERT_NE(out[0], nullptr);
        ASSERT_NE(out[1], nullptr);
        ASSERT_EQ(*out[0], 1);
        ASSERT_EQ(*out[1], 2);
    }
}


struct Period
{
    std::chrono::milliseconds startTimeMs{};
    std::chrono::milliseconds durationMs{};

    friend bool operator==(const Period&, const Period&) = default;
};

// GTest printer
[[maybe_unused]] void PrintTo(const Period& p, std::ostream* os)
{
    *os << "Period{start=" << p.startTimeMs.count() << "ms"
        << ", duration=" << p.durationMs.count() << "ms}";
}

using Intersection = nx::views::IntersectingPair<Period, Period>;

[[maybe_unused]] bool operator==(const Intersection& a, const Intersection& b)
{
    return a.first == b.first && a.second == b.second;
}

// GTest printer
[[maybe_unused]] void PrintTo(const Intersection& x, std::ostream* os)
{
    *os << "Intersection{first=";
    PrintTo(x.first, os);
    *os << ", second=";
    PrintTo(x.second, os);
    *os << "}";
}

struct IntersectionsCase
{
    std::vector<Period> left{};
    std::vector<Period> right{};
    std::vector<Intersection> expected{};
    std::size_t upToN{};
};

// GTest printer
[[maybe_unused]] void PrintTo(const IntersectionsCase& c, std::ostream* os)
{
    *os << "IntersectionsCase{left.size=" << c.left.size()
        << ", right.size=" << c.right.size()
        << ", expected.size=" << c.expected.size()
        << ", upToN=" << c.upToN << "}";
}

struct SelfIntersectionsCase
{
    std::vector<Period> input{};
    std::vector<Intersection> expected{};
    std::size_t upToN{};
};

// GTest printer
[[maybe_unused]] void PrintTo(const SelfIntersectionsCase& c, std::ostream* os)
{
    *os << "SelfIntersectionsCase{input.size=" << c.input.size()
        << ", expected.size=" << c.expected.size()
        << ", upToN=" << c.upToN << "}";
}

class IntersectionsSameContainer: public ::testing::TestWithParam<IntersectionsCase> {};

TEST_P(IntersectionsSameContainer, simple)
{
    const auto& [left, right, expected, upToN] = GetParam();

    EXPECT_EQ(
        (left
         | nx::views::intersections(right)
         | std::views::take(upToN)
         | nx::ranges::to<std::vector>()),
        expected);

    EXPECT_EQ(
        (nx::views::intersections(left, right)
         | std::views::take(upToN)
         | nx::ranges::to<std::vector>()),
        expected);
}

const std::vector<IntersectionsCase> kIntersectionsSameContainerCases{
    {
        .left = {},
        .right = {},
        .expected = {},
        .upToN = 10,
    },
    {
        .left =
            {
                Period{0ms, 10ms},
            },
        .right =
            {
                Period{0ms, 10ms},
            },
        .expected =
            {
                Intersection{
                    .first = Period{0ms, 10ms},
                    .second = Period{0ms, 10ms},
                },
            },
        .upToN = 10,
    },
    {
        .left =
            {
                Period{0ms, 10ms},
            },
        .right =
            {
                Period{10ms, 10ms},
            },
        .expected = {},
        .upToN = 10,
    },
    {
        .left =
            {
                Period{10ms, 10ms},
                Period{30ms, 10ms},
                Period{70ms, 10ms},
                Period{120ms, 10ms},
            },
        .right =
            {
                Period{0ms, 100ms},
            },
        .expected =
            {
                Intersection{
                    .first = Period{10ms, 10ms},
                    .second = Period{0ms, 100ms},
                },
                Intersection{
                    .first = Period{30ms, 10ms},
                    .second = Period{0ms, 100ms},
                },
                Intersection{
                    .first = Period{70ms, 10ms},
                    .second = Period{0ms, 100ms},
                },
            },
        .upToN = 10,
    },
    {
        .left =
            {
                Period{10ms, 10ms},
                Period{30ms, 10ms},
                Period{70ms, 10ms},
                Period{120ms, 10ms},
            },
        .right =
            {
                Period{0ms, 100ms},
            },
        .expected =
            {
                Intersection{
                    .first = Period{10ms, 10ms},
                    .second = Period{0ms, 100ms},
                },
                Intersection{
                    .first = Period{30ms, 10ms},
                    .second = Period{0ms, 100ms},
                },
            },
        .upToN = 2,
    },
    {
        .left =
            {
                Period{0ms, 10ms},
                Period{20ms, 10ms},
                Period{40ms, 10ms},
            },
        .right =
            {
                Period{5ms, 50ms},
                Period{60ms, 10ms},
            },
        .expected =
            {
                Intersection{
                    .first = Period{0ms, 10ms},
                    .second = Period{5ms, 50ms},
                },
                Intersection{
                    .first = Period{20ms, 10ms},
                    .second = Period{5ms, 50ms},
                },
                Intersection{
                    .first = Period{40ms, 10ms},
                    .second = Period{5ms, 50ms},
                },
            },
        .upToN = 999,
    },
};

INSTANTIATE_TEST_SUITE_P(
    ranges,
    IntersectionsSameContainer,
    ::testing::ValuesIn(kIntersectionsSameContainerCases));

class SelfIntersections: public ::testing::TestWithParam<SelfIntersectionsCase> {};

TEST_P(SelfIntersections, simple)
{
    const auto& [input, expected, upToN] = GetParam();

    EXPECT_EQ(
        (input
         | nx::views::selfIntersections
         | std::views::take(upToN)
         | nx::ranges::to<std::vector>()),
        expected);
}

const std::vector<SelfIntersectionsCase> kSelfIntersectionsCases{
    {
        .input = {
        },
        .expected = {
        },
        .upToN = 10,
    },
    {
        .input = {
            Period{0ms, 10ms},
        },
        .expected = {
        },
        .upToN = 10,
    },
    {
        .input = {
            Period{0ms, 10ms},
            Period{20ms, 5ms},
        },
        .expected = {
        },
        .upToN = 10,
    },
    {
        .input = {
            Period{0ms, 10ms},
            Period{5ms, 10ms},
        },
        .expected = {
            Intersection{
                .first = Period{0ms, 10ms},
                .second = Period{5ms, 10ms},
            },
        },
        .upToN = 10,
    },
    {
        .input = {
            Period{0ms, 10ms},
            Period{5ms, 10ms},
            Period{12ms, 10ms},
            Period{18ms, 10ms},
        },
        .expected = {
            Intersection{
                .first = Period{0ms, 10ms},
                .second = Period{5ms, 10ms},
            },
            Intersection{
                .first = Period{5ms, 10ms},
                .second = Period{12ms, 10ms},
            },
        },
        .upToN = 2,
    },
};

INSTANTIATE_TEST_SUITE_P(
    ranges,
    SelfIntersections,
    ::testing::ValuesIn(kSelfIntersectionsCases));

struct PeriodWithOffset
{
    std::chrono::milliseconds startTimeMs{};
    std::chrono::milliseconds durationMs{};
    std::chrono::milliseconds offsetMs{};

    friend bool operator==(const PeriodWithOffset&, const PeriodWithOffset&) = default;
};

// GTest printer
[[maybe_unused]] void PrintTo(const PeriodWithOffset& p, std::ostream* os)
{
    *os << "PeriodWithOffset{start=" << p.startTimeMs.count() << "ms"
        << ", duration=" << p.durationMs.count() << "ms"
        << ", offset=" << p.offsetMs.count() << "ms}";
}

struct CompatiblePeriod
{
    std::chrono::milliseconds startTimeMs{};
    std::chrono::milliseconds durationMs{};

    friend bool operator==(const CompatiblePeriod&, const CompatiblePeriod&) = default;
};

// GTest printer
[[maybe_unused]] void PrintTo(const CompatiblePeriod& p, std::ostream* os)
{
    *os << "CompatiblePeriod{start=" << p.startTimeMs.count() << "ms"
        << ", duration=" << p.durationMs.count() << "ms}";
}

using MixedIntersection = nx::views::IntersectingPair<Period, CompatiblePeriod>;

[[maybe_unused]] bool operator==(const MixedIntersection& a, const MixedIntersection& b)
{
    return a.first == b.first && a.second == b.second;
}

// GTest printer
[[maybe_unused]] void PrintTo(const MixedIntersection& x, std::ostream* os)
{
    *os << "MixedIntersection{first=";
    PrintTo(x.first, os);
    *os << ", second=";
    PrintTo(x.second, os);
    *os << "}";
}

struct IntersectionsTransformedCase
{
    std::vector<Period> left{};
    std::vector<PeriodWithOffset> right{};
    std::vector<MixedIntersection> expected{};
    std::size_t upToN{};
};

[[maybe_unused]] void PrintTo(const IntersectionsTransformedCase& c, std::ostream* os)
{
    *os << "IntersectionsTransformedCase{left.size=" << c.left.size()
        << ", right.size=" << c.right.size()
        << ", expected.size=" << c.expected.size()
        << ", upToN=" << c.upToN << "}";
}

class IntersectionsTransformedViews: public ::testing::TestWithParam<IntersectionsTransformedCase> {};

TEST_P(IntersectionsTransformedViews, simple)
{
    const auto& [left, right, expected, upToN] = GetParam();

    EXPECT_EQ(
        (left
         // Additional dummy transform for compilation check
         | std::views::transform(
             [](const Period& p)
             {
                 return p;
             })
         | std::views::filter(
             [](const Period& p)
             {
                 return p.durationMs > 0ms;
             })
         | nx::views::intersections(
             right
             | std::views::filter(
                 [](const PeriodWithOffset& p)
                 {
                     return p.durationMs > 0ms;
                 })
             | std::views::transform(
                 [](const PeriodWithOffset& p)
                 {
                     return CompatiblePeriod{
                         .startTimeMs = p.startTimeMs + p.offsetMs,
                         .durationMs = p.durationMs,
                     };
                 }))
         | std::views::take(upToN)
         | nx::ranges::to<std::vector>()),
        expected);

    EXPECT_EQ(
        (nx::views::intersections(
             left
                 // Additional dummy transform for compilation check
                 | std::views::transform(
                     [](const Period& p)
                     {
                         return p;
                     })
                 | std::views::filter(
                     [](const Period& p)
                     {
                         return p.durationMs > 0ms;
                     }),
             right
                 | std::views::filter(
                     [](const PeriodWithOffset& p)
                     {
                         return p.durationMs > 0ms;
                     })
                 | std::views::transform(
                     [](const PeriodWithOffset& p)
                     {
                         return CompatiblePeriod{
                             .startTimeMs = p.startTimeMs + p.offsetMs,
                             .durationMs = p.durationMs,
                         };
                     }))
         | std::views::take(upToN) | nx::ranges::to<std::vector>()),
        expected);
}

const std::vector<IntersectionsTransformedCase> kIntersectionsTransformedCases{{
    {
        .left = {},
        .right = {},
        .expected = {},
        .upToN = 10,
    },
    {
        .left =
            {
                Period{0ms, 10ms},
                Period{20ms, 10ms},
                Period{40ms, 10ms},
            },
        .right =
            {
                PeriodWithOffset{5ms, 10ms, 0ms},
                PeriodWithOffset{25ms, 10ms, -5ms},
                PeriodWithOffset{60ms, 5ms, 0ms},
            },
        .expected =
            {
                MixedIntersection{
                    .first = Period{0ms, 10ms},
                    .second = CompatiblePeriod{5ms, 10ms},
                },
                MixedIntersection{
                    .first = Period{20ms, 10ms},
                    .second = CompatiblePeriod{20ms, 10ms},
                },
            },
        .upToN = 999,
    },
    {
        .left =
            {
                Period{0ms, 10ms},
                Period{20ms, 10ms},
                Period{40ms, 10ms},
            },
        .right =
            {
                PeriodWithOffset{5ms, 10ms, 0ms},
                PeriodWithOffset{25ms, 10ms, -5ms},
                PeriodWithOffset{60ms, 5ms, 0ms},
            },
        .expected =
            {
                MixedIntersection{
                    .first = Period{0ms, 10ms},
                    .second = CompatiblePeriod{5ms, 10ms},
                },
            },
        .upToN = 1,
    },
    {
        .left =
            {
                Period{0ms, 10ms},
                Period{15ms, 0ms},
                Period{20ms, 10ms},
                Period{40ms, 10ms},
            },
        .right =
            {
                PeriodWithOffset{5ms, 10ms, 0ms},
                PeriodWithOffset{25ms, 0ms, 0ms},
                PeriodWithOffset{25ms, 10ms, -5ms},
            },
        .expected =
            {
                MixedIntersection{
                    .first = Period{0ms, 10ms},
                    .second = CompatiblePeriod{5ms, 10ms},
                },
                MixedIntersection{
                    .first = Period{20ms, 10ms},
                    .second = CompatiblePeriod{20ms, 10ms},
                },
            },
        .upToN = 999,
    },
}};

INSTANTIATE_TEST_SUITE_P(
    ranges,
    IntersectionsTransformedViews,
    ::testing::ValuesIn(kIntersectionsTransformedCases));

} // namespace
} // namespace nx::utils::test
