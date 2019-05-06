#include <gtest/gtest.h>

#include <nx/vms/server/analytics/detected_objects_storage/rect_aggregator.h>

namespace nx::analytics::storage::test {

using AggregatedRect = storage::RectAggregator<int>::AggregatedRect;

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
    storage::RectAggregator<int> m_aggregator;

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
        AggregatedRect{QRect(9, 9, 5, 5), {1, 2}} };

    const auto actual = aggregate(
        AggregatedRect{QRect(1, 1, 5, 5), {1}},
        AggregatedRect{QRect(1, 1, 5, 5), {2}},
        AggregatedRect{QRect(9, 9, 5, 5), {1}},
        AggregatedRect{QRect(9, 9, 5, 5), {2}}
    );

    assertEqual(expected, actual);
}

} // namespace nx::analytics::storage::test
