// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/utils/random.h>

#include <utils/math/rect_aggregator.h>

namespace nx::analytics::db::test {

using AggregatedRect = RectAggregator<int>::AggregatedRect;

/**
 * NOTE: This comparator makes sense for this test only.
 */
struct RectComparator
{
    bool operator()(const AggregatedRect& left, const AggregatedRect& right) const
    {
        if (left.rect.x() < right.rect.x())
            return true;
        if (left.rect.x() > right.rect.x())
            return false;

        if (left.rect.y() < right.rect.y())
            return true;
        if (left.rect.y() > right.rect.y())
            return false;

        if (left.rect.width() < right.rect.width())
            return true;
        if (left.rect.width() > right.rect.width())
            return false;

        return left.rect.height() < right.rect.height();
    }
};

class RectAggregator:
    public ::testing::Test
{
protected:
    std::vector<AggregatedRect> generateRandomRects(
        int count,
        int uniqueValueCount)
    {
        // Generating random values.
        std::vector<int> values(uniqueValueCount);
        std::generate(values.begin(), values.end(), &rand);

        const QRect maxRect(0, 0, 100, 100);

        std::vector<AggregatedRect> data;
        for (int i = 0; i < count; ++i)
        {
            QRect rect(
                nx::utils::random::number<int>(0, 100),
                nx::utils::random::number<int>(0, 100),
                nx::utils::random::number<int>(0, 100),
                nx::utils::random::number<int>(0, 100));
            rect = rect.intersected(maxRect);

            data.push_back({rect, {nx::utils::random::choice(values)}});
        }

        return data;
    }

    std::vector<AggregatedRect> aggregate(const std::vector<AggregatedRect>& data)
    {
        for (const auto& item: data)
            m_aggregator.add(item.rect, *item.values.begin());
        return m_aggregator.aggregatedData();
    }

    template<typename ... Args>
    std::vector<AggregatedRect> aggregate(const Args&... args)
    {
        aggregateInternal(args...);
        return m_aggregator.aggregatedData();
    }

    void assertEqual(
        std::vector<AggregatedRect> left,
        std::vector<AggregatedRect> right)
    {
        std::sort(left.begin(), left.end(), RectComparator());
        std::sort(right.begin(), right.end(), RectComparator());

        ASSERT_EQ(left, right);
    }

private:
    ::RectAggregator<int> m_aggregator;

    template<typename ... Args>
    void aggregateInternal(const AggregatedRect& data, const Args&... args)
    {
        ASSERT_EQ(1, data.values.size());
        m_aggregator.add(data.rect, *data.values.begin());

        if constexpr (sizeof...(Args) > 0)
            aggregateInternal(args...);
    }
};

TEST_F(RectAggregator, same_rect_repeated_multiple_times_is_aggregated)
{
    /*
     1 1
     1 1
     */

    const auto expected = std::vector<AggregatedRect>{
        AggregatedRect{QRect(1, 1, 2, 2), {1, 2, 3}}};

    const auto actual = aggregate(
        AggregatedRect{QRect(1, 1, 2, 2), {1}},
        AggregatedRect{QRect(1, 1, 2, 2), {2}},
        AggregatedRect{QRect(1, 1, 2, 2), {3}});

    assertEqual(expected, actual);
}

TEST_F(RectAggregator, example2)
{
    /*
     1 1 3
     1 1 3
     */

    const auto expected = std::vector<AggregatedRect>{
        AggregatedRect{QRect(1, 1, 2, 2), {1, 2, 3}},
        AggregatedRect{QRect(3, 1, 1, 2), {3}}};

    const auto actual = aggregate(
        AggregatedRect{QRect(1, 1, 2, 2), {1}},
        AggregatedRect{QRect(1, 1, 2, 2), {2}},
        AggregatedRect{QRect(1, 1, 3, 2), {3}});

    assertEqual(expected, actual);
}

TEST_F(RectAggregator, example3)
{
    /*
     1 1 1 1 1
     1 2 2 2 1
     1 2 2 2 1
     1 2 2 2 1
     1 1 1 1 1
     */

    const auto expected = std::vector<AggregatedRect>{
        AggregatedRect{QRect(2, 2, 3, 3), {1, 2}},
        AggregatedRect{QRect(1, 1, 5, 1), {1}},
        AggregatedRect{QRect(1, 5, 5, 1), {1}},
        AggregatedRect{QRect(1, 2, 1, 3), {1}},
        AggregatedRect{QRect(5, 2, 1, 3), {1}}};

    const auto actual = aggregate(
        AggregatedRect{QRect(1, 1, 5, 5), {1}},
        AggregatedRect{QRect(2, 2, 3, 3), {2}});

    assertEqual(expected, actual);
}

TEST_F(RectAggregator, rects_with_same_value_are_aggregated)
{
    const auto expected = std::vector<AggregatedRect>{
        AggregatedRect{QRect(1, 1, 5, 5), {1, 2}},
        AggregatedRect{QRect(9, 9, 5, 5), {1, 2}}};

    const auto actual = aggregate(
        AggregatedRect{QRect(1, 1, 5, 5), {1}},
        AggregatedRect{QRect(1, 1, 5, 5), {2}},
        AggregatedRect{QRect(9, 9, 5, 5), {1}},
        AggregatedRect{QRect(9, 9, 5, 5), {2}}
    );

    assertEqual(expected, actual);
}

TEST_F(RectAggregator, adjacent_rects_with_same_values_are_merged)
{
    const auto expected = std::vector<AggregatedRect>{
        AggregatedRect{QRect(19, 13, 2, 1), {1}}};

    const auto actual = aggregate(
        AggregatedRect{QRect(19, 13, 1, 1), {1}},
        AggregatedRect{QRect(20, 13, 1, 1), {1}}
    );

    assertEqual(expected, actual);
}

TEST_F(RectAggregator, produced_adjacent_rects_with_same_values_are_merged)
{
    const auto expected = std::vector<AggregatedRect>{
        AggregatedRect{QRect(1, 1, 12, 1), {1}}};

    const auto actual = aggregate(
        AggregatedRect{QRect(1, 1, 10, 1), {1}},
        AggregatedRect{QRect(2, 1, 10, 1), {1}},
        AggregatedRect{QRect(12, 1, 1, 1), {1}}
    );

    assertEqual(expected, actual);
}

TEST_F(RectAggregator, stress_test)
{
    aggregate(generateRandomRects(150, 5));
}

} // namespace nx::analytics::db::test
