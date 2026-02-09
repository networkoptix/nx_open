// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <ranges>

#include <nx/concepts/common.h>
#include <nx/concepts/ranges.h>

namespace nx::views {

// Element type of `intersections` result view
template<typename Left, typename Right>
struct IntersectingPair
{
    Left first;
    Right second;
};

/*
Lazy view enumerating overlaps between two time-period ranges using a two-pointer sweep.

Assumptions for correctness:
- Both input ranges are ordered by nondecreasing startTimeMs.

Semantics:
- Periods are treated as half-open intervals: [startTimeMs, startTimeMs + durationMs).
- Touching at boundary (end == other.start) is NOT an intersection.

Access patterns:
- intersections(lhs, rhs)
- lhs | intersections(rhs)
*/
template<std::ranges::view RangeFirst, std::ranges::view RangeSecond>
    requires TimePeriodLike<std::ranges::range_value_t<RangeFirst>>
    && TimePeriodLike<std::ranges::range_value_t<RangeSecond>>
class Intersections: public std::ranges::view_interface<Intersections<RangeFirst, RangeSecond>>
{
    using SubrangeFirst = std::ranges::
        subrange<std::ranges::iterator_t<RangeFirst>, std::ranges::sentinel_t<RangeFirst>>;

    using SubrangeSecond = std::ranges::
        subrange<std::ranges::iterator_t<RangeSecond>, std::ranges::sentinel_t<RangeSecond>>;

    using LeftPeriod = std::ranges::range_value_t<RangeFirst>;
    using RightPeriod = std::ranges::range_value_t<RangeSecond>;
    using Intersection = nx::views::IntersectingPair<LeftPeriod, RightPeriod>;

    struct Iterator
    {
        SubrangeFirst firstRemaining;
        SubrangeSecond secondRemaining;
        std::optional<Intersection> nextIntersection;

        using iterator_concept = std::input_iterator_tag;
        using iterator_category = std::input_iterator_tag;
        using value_type = Intersection;
        using difference_type = std::ptrdiff_t;

        constexpr const value_type& operator*() const noexcept { return *nextIntersection; }

        constexpr Iterator& operator++()
        {
            *this = Intersections::computeNextIntersection(firstRemaining, secondRemaining);
            return *this;
        }

        constexpr void operator++(int) { ++(*this); }

        constexpr bool operator==(std::default_sentinel_t) const noexcept
        {
            return !nextIntersection.has_value();
        }
    };

    static constexpr Iterator computeNextIntersection(
        SubrangeFirst firstRemaining,
        SubrangeSecond secondRemaining)
    {
        while (firstRemaining.begin() != firstRemaining.end()
               && secondRemaining.begin() != secondRemaining.end())
        {
            const auto firstIterator = firstRemaining.begin();
            const auto secondIterator = secondRemaining.begin();

            const auto& firstPeriod = *firstIterator;
            const auto& secondPeriod = *secondIterator;

            const std::chrono::milliseconds firstStart{firstPeriod.startTimeMs};
            const std::chrono::milliseconds firstEnd =
                firstStart + std::chrono::milliseconds{firstPeriod.durationMs};
            const std::chrono::milliseconds secondStart{secondPeriod.startTimeMs};
            const std::chrono::milliseconds secondEnd =
                secondStart + std::chrono::milliseconds{secondPeriod.durationMs};

            if (firstEnd <= secondStart)
            {
                firstRemaining = SubrangeFirst{std::next(firstIterator), firstRemaining.end()};
                continue;
            }

            if (secondEnd <= firstStart)
            {
                secondRemaining = SubrangeSecond{std::next(secondIterator), secondRemaining.end()};
                continue;
            }

            const bool advanceFirst = firstEnd <= secondEnd;
            const bool advanceSecond = secondEnd <= firstEnd;

            return {
                .firstRemaining = advanceFirst
                    ? SubrangeFirst{std::next(firstIterator), firstRemaining.end()}
                    : std::move(firstRemaining),
                .secondRemaining = advanceSecond
                    ? SubrangeSecond{std::next(secondIterator), secondRemaining.end()}
                    : std::move(secondRemaining),
                .nextIntersection =
                    Intersection{
                        .first = firstPeriod,
                        .second = secondPeriod,
                    },
            };
        }

        return {
            .firstRemaining = std::move(firstRemaining),
            .secondRemaining = std::move(secondRemaining),
            .nextIntersection = std::nullopt,
        };
    }

public:
    constexpr Intersections(RangeFirst firstView, RangeSecond secondView):
        storedFirstView(std::move(firstView)),
        storedSecondView(std::move(secondView))
    {
    }

    constexpr Iterator begin()
    {
        return computeNextIntersection(
            SubrangeFirst{std::ranges::begin(storedFirstView), std::ranges::end(storedFirstView)},
            SubrangeSecond{
                std::ranges::begin(storedSecondView),
                std::ranges::end(storedSecondView)});
    }

    [[nodiscard]] constexpr std::default_sentinel_t end() const noexcept { return {}; }

private:
    RangeFirst storedFirstView;
    RangeSecond storedSecondView;
};

template<std::ranges::view V1, std::ranges::view V2>
Intersections(V1, V2) -> Intersections<V1, V2>;

template<std::ranges::range R1, std::ranges::range R2>
    requires TimePeriodLike<std::ranges::range_value_t<R1>>
    && TimePeriodLike<std::ranges::range_value_t<R2>>
Intersections(R1&, R2&) -> Intersections<std::ranges::ref_view<R1>, std::ranges::ref_view<R2>>;

template<class RhsView>
struct IntersectionsClosure
{
    RhsView rhsView;

    template<std::ranges::viewable_range Lhs>
        requires TimePeriodLike<std::ranges::range_value_t<Lhs>>
        && TimePeriodLike<std::ranges::range_value_t<RhsView>>
    constexpr auto operator()(Lhs&& lhs) const
    {
        return Intersections{
            std::views::all(std::forward<Lhs>(lhs)),
            rhsView,
        };
    }

    template<std::ranges::viewable_range Lhs>
        requires TimePeriodLike<std::ranges::range_value_t<Lhs>>
        && TimePeriodLike<std::ranges::range_value_t<RhsView>>
    friend constexpr auto operator|(Lhs&& lhs, const IntersectionsClosure& self)
    {
        return self(std::forward<Lhs>(lhs));
    }
};

template<std::ranges::viewable_range Lhs, std::ranges::viewable_range Rhs>
    requires TimePeriodLike<std::ranges::range_value_t<Lhs>>
    && TimePeriodLike<std::ranges::range_value_t<Rhs>>
constexpr auto intersections(Lhs&& lhs, Rhs&& rhs)
{
    return Intersections{
        std::views::all(std::forward<Lhs>(lhs)),
        std::views::all(std::forward<Rhs>(rhs)),
    };
}

template<std::ranges::viewable_range Rhs>
    requires TimePeriodLike<std::ranges::range_value_t<Rhs>>
constexpr auto intersections(Rhs&& rhs)
{
    return IntersectionsClosure<std::views::all_t<Rhs>>{
        std::views::all(std::forward<Rhs>(rhs)),
    };
}

struct SelfIntersections
{
    template<std::ranges::viewable_range R>
        requires TimePeriodLike<std::ranges::range_value_t<R>>
    constexpr auto operator()(R&& r) const
    {
        auto view = std::views::all(std::forward<R>(r));

        return std::views::zip(view, view | std::views::drop(1))
            | std::views::filter(
                   [](const auto& tuple)
                   {
                       const auto& [a, b] = tuple;
                       return !(
                           a.startTimeMs + a.durationMs <= b.startTimeMs
                           || b.startTimeMs + b.durationMs <= a.startTimeMs);
                   })
            | std::views::transform(
                   [](const auto& tuple)
                   {
                       return nx::views::IntersectingPair{
                           .first = std::get<0>(tuple),
                           .second = std::get<1>(tuple),
                       };
                   });
    }

    template<std::ranges::viewable_range R>
        requires TimePeriodLike<std::ranges::range_value_t<R>>
    friend constexpr auto operator|(R&& r, SelfIntersections self)
    {
        return self(std::forward<R>(r));
    }
};

/*
selfIntersections

Equivalent to: filter(zip(v, drop(v, 1)), overlaps).
*/
constexpr SelfIntersections selfIntersections{};

} // namespace nx::views
