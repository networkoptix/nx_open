#include <gtest/gtest.h>

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

using AggregatedRect1 = ::RectAggregator<std::string>::AggregatedRect;

static const AggregatedRect1 rects2[] = {
    AggregatedRect1{ QRect(9,10, 7, 5), {"29a86763-4800-422b-8357-45bc6dfd1359"}},
    AggregatedRect1{ QRect(9,10, 8, 4), {"29a86763-4800-422b-8357-45bc6dfd1359"}},
    AggregatedRect1{ QRect(9,9, 8, 5), {"29a86763-4800-422b-8357-45bc6dfd1359"}},
    AggregatedRect1{ QRect(9,8, 8, 5), {"29a86763-4800-422b-8357-45bc6dfd1359"}},
    AggregatedRect1{ QRect(9,7, 8, 5), {"29a86763-4800-422b-8357-45bc6dfd1359"}},
    AggregatedRect1{ QRect(9,6, 8, 5), {"29a86763-4800-422b-8357-45bc6dfd1359"}},
    AggregatedRect1{ QRect(9,5, 8, 5), {"29a86763-4800-422b-8357-45bc6dfd1359"}},
    AggregatedRect1{ QRect(9,4, 8, 5), {"29a86763-4800-422b-8357-45bc6dfd1359"}},
    AggregatedRect1{ QRect(9,3, 8, 5), {"29a86763-4800-422b-8357-45bc6dfd1359"}},
    AggregatedRect1{ QRect(9,2, 8, 5), {"29a86763-4800-422b-8357-45bc6dfd1359"}},
    AggregatedRect1{ QRect(9,2, 8, 4), {"29a86763-4800-422b-8357-45bc6dfd1359"}},
    AggregatedRect1{ QRect(10,1, 7, 5), {"29a86763-4800-422b-8357-45bc6dfd1359"}},
    AggregatedRect1{ QRect(10,0, 7, 5), {"29a86763-4800-422b-8357-45bc6dfd1359"}},
    AggregatedRect1{ QRect(6,6, 11, 6), {"25c66495-6fde-4fd5-9fb2-214747b106de"}},
    AggregatedRect1{ QRect(5,7, 12, 6), {"25c66495-6fde-4fd5-9fb2-214747b106de"}},
    AggregatedRect1{ QRect(4,7, 12, 6), {"25c66495-6fde-4fd5-9fb2-214747b106de"}},
    AggregatedRect1{ QRect(4,8, 11, 6), {"25c66495-6fde-4fd5-9fb2-214747b106de"}},
    AggregatedRect1{ QRect(5,8, 11, 6), {"25c66495-6fde-4fd5-9fb2-214747b106de"}},
    AggregatedRect1{ QRect(6,8, 12, 6), {"25c66495-6fde-4fd5-9fb2-214747b106de"}},
    AggregatedRect1{ QRect(7,8, 12, 6), {"25c66495-6fde-4fd5-9fb2-214747b106de"}},
    AggregatedRect1{ QRect(9,8, 11, 6), {"25c66495-6fde-4fd5-9fb2-214747b106de"}},
    AggregatedRect1{ QRect(10,8, 11, 6), {"25c66495-6fde-4fd5-9fb2-214747b106de"}},
    AggregatedRect1{ QRect(11,8, 12, 6), {"25c66495-6fde-4fd5-9fb2-214747b106de"}},
    AggregatedRect1{ QRect(12,7, 12, 7), {"25c66495-6fde-4fd5-9fb2-214747b106de"}},
    AggregatedRect1{ QRect(14,7, 11, 6), {"25c66495-6fde-4fd5-9fb2-214747b106de"}},
    AggregatedRect1{ QRect(15,7, 11, 6), {"25c66495-6fde-4fd5-9fb2-214747b106de"}},
    AggregatedRect1{ QRect(16,7, 12, 6), {"25c66495-6fde-4fd5-9fb2-214747b106de"}},
    AggregatedRect1{ QRect(17,7, 12, 6), {"25c66495-6fde-4fd5-9fb2-214747b106de"}},
    AggregatedRect1{ QRect(19,7, 11, 6), {"25c66495-6fde-4fd5-9fb2-214747b106de"}},
    AggregatedRect1{ QRect(20,7, 11, 6), {"25c66495-6fde-4fd5-9fb2-214747b106de"}},
    AggregatedRect1{ QRect(21,7, 12, 6), {"25c66495-6fde-4fd5-9fb2-214747b106de"}},
    AggregatedRect1{ QRect(22,7, 12, 6), {"25c66495-6fde-4fd5-9fb2-214747b106de"}},
    AggregatedRect1{ QRect(24,7, 11, 6), {"25c66495-6fde-4fd5-9fb2-214747b106de"}},
    AggregatedRect1{ QRect(25,7, 11, 6), {"25c66495-6fde-4fd5-9fb2-214747b106de"}},
    AggregatedRect1{ QRect(26,7, 12, 6), {"25c66495-6fde-4fd5-9fb2-214747b106de"}},
    AggregatedRect1{ QRect(27,7, 12, 6), {"25c66495-6fde-4fd5-9fb2-214747b106de"}},
    AggregatedRect1{ QRect(29,7, 11, 6), {"25c66495-6fde-4fd5-9fb2-214747b106de"}},
    AggregatedRect1{ QRect(30,7, 11, 6), {"25c66495-6fde-4fd5-9fb2-214747b106de"}},
    AggregatedRect1{ QRect(31,7, 12, 6), {"25c66495-6fde-4fd5-9fb2-214747b106de"}},
    AggregatedRect1{ QRect(32,7, 12, 6), {"25c66495-6fde-4fd5-9fb2-214747b106de"}},
    AggregatedRect1{ QRect(25,11, 19, 11), {"09357097-1ce5-49cd-a210-5f0e6eaf241b"}},
    AggregatedRect1{ QRect(25,10, 19, 11), {"09357097-1ce5-49cd-a210-5f0e6eaf241b"}},
    AggregatedRect1{ QRect(17,12, 12, 8), {"31ba2512-130e-4342-8d9f-614d42e4596a"}},
    AggregatedRect1{ QRect(17,12, 12, 8), {"31ba2512-130e-4342-8d9f-614d42e4596a"}},
    AggregatedRect1{ QRect(16,13, 12, 8), {"31ba2512-130e-4342-8d9f-614d42e4596a"}},
    AggregatedRect1{ QRect(15,13, 12, 8), {"31ba2512-130e-4342-8d9f-614d42e4596a"}},
    AggregatedRect1{ QRect(15,13, 11, 8), {"31ba2512-130e-4342-8d9f-614d42e4596a"}}
};

TEST_F(RectAggregator, xxx)
{
    ::RectAggregator<std::string> aggregator;

    for (const auto& item: rects2)
        aggregator.add(item.rect, *item.values.begin());
}

} // namespace nx::analytics::db::test
