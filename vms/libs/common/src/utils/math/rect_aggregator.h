#pragma once

#include <set>

#include <QtCore/QRect>
#include <QtGui/QRegion>

#include <nx/utils/log/assert.h>
#include <nx/utils/log/log.h>

/**
 * Aggregates / splits provided rectangles so that the result does not contain
 * interlapping rectangles while preserving association between area and some value.
 * Note that it is possible that the number of output rectangles is greater
 * than number of input rectangles.
 *
 * Example 1:
 * Input: {r{1;1;3;3}, v1}, {r{1;1;3;3}, v2}, {r{1;1;3;3}, v3}.
 * Output: {r{1;1;3;3}, {v1, v2, v3}}.
 *
 * Example 2:
 * Input: {r{1;1;3;3}, v1}, {r{1;1;3;3}, v2}, {r{1;1;4;3}, v3}.
 * Output: {r{1;1;3;3}, {v1, v2, v3}}, {r{3;1;4;3}, {v3}}.
 *
 * Example 3 (extreme case: output is larger than input):
 * Input: {r{1;1;5;5}, v1}, {r{2;2;4;4}, v2}.
 * Output: {r{1;1;5;2}, v1}, {r{4;1;5;5}, v1}, {r{1;4;5;5}, v1}, {r{1;1;2;5}, v1}, {r{2;2;4;4}, {v1, v2}}.
 *
 * NOTE: All aggregation happens when adding data.
 */
template<typename Value>
class RectAggregator
{
public:
    struct AggregatedRect
    {
        QRect rect;
        std::set<Value> values;

        bool operator==(const AggregatedRect& right) const
        {
            return rect == right.rect && values == right.values;
        }
    };

    void add(const QRect& rect, const Value& value)
    {
        NX_VERBOSE(this, "Adding rect %1 with value %2", rect, value);

        QRegion region(rect);

        for (int i = 0; i < (int) m_aggregatedRects.size(); ++i)
        {
            auto& aggregatedRect = m_aggregatedRects[i];
            if (!rect.intersects(aggregatedRect.rect))
                continue;

            const auto intersectionRegion = region.intersected(aggregatedRect.rect);
            if (intersectionRegion.isEmpty())
                continue;

            // It is always so since m_aggregatedRects does not contain intersecting rects.
            NX_ASSERT(intersectionRegion.rectCount() == 1,
                lm("intersectionRegion.rectCount() = %1").args(intersectionRegion.rectCount()));

            const auto intersection = *intersectionRegion.begin();
            if (aggregatedRect.values.find(value) == aggregatedRect.values.end())
            {
                // Have to create new rect in m_aggregatedRects with (old values + new value).
                if (intersection != aggregatedRect.rect)
                {
                    splitRectInplace(m_aggregatedRects.begin() + i, intersection, value);
                    // NOTE: As a result of splitting, the number of rects can actually decrease
                    // since new rects can be merged to existing ones. Example:
                    // 1 1 1 3 3 3                                              1 1 1 1 1 1
                    // 1 1 1 3 3 3  splitting rect 3 in the middle may produce  1 1 1 1 1 1
                    // 2 2 2 3 3 3                                              2 2 2 2 2 2
                    // 2 2 2 3 3 3                                              2 2 2 2 2 2
                    // Rect 3 has been merged into rects 1 & 2. So, rect count has actually decreased.
                    // To address it, processing the current index once more.
                    // We cannot deadloop here because the region is decreased by non-empty intersection
                    // at every iteration.
                    --i;
                }
                else
                {
                    aggregatedRect.values.insert(value);
                }
            }
            region -= intersection;

            if (region.isEmpty())
                return;
        }

        for (const auto& newRect: region)
            insertNonOverlappingRect(m_aggregatedRects.end(), {newRect, {value}});
    }

    const std::vector<AggregatedRect>& aggregatedData() const
    {
        return m_aggregatedRects;
    }

    std::vector<AggregatedRect> takeAggregatedData()
    {
        NX_VERBOSE(this, "Giving out %1 aggregated rects", m_aggregatedRects.size());

        return std::exchange(m_aggregatedRects, {});
    }

    void clear()
    {
        NX_VERBOSE(this, "Clear");

        m_aggregatedRects.clear();
    }

    /**
     * @return Number of produced rectangles so far.
     */
    std::size_t size() const
    {
        return m_aggregatedRects.size();
    }

private:
    using AggregatedRects = std::vector<AggregatedRect>;

    AggregatedRects m_aggregatedRects;

    /**
     * Splits {rect, values} (pointed to by rectIter) to
     * {newRect, values|newValue} and {rect-newRect, values}.
     * If {rect-newRect} is not a rect, it is expanded to set of rects which constitute it.
     * @return Iterator pointing to the rect equal to requiredSubRect.
     */
    typename AggregatedRects::iterator splitRectInplace(
        typename AggregatedRects::iterator rectIter,
        const QRect& newRect,
        const Value& newValue)
    {
        NX_ASSERT(rectIter->rect.contains(newRect));

        const auto posToInsertAt = rectIter - m_aggregatedRects.begin();
        const auto aggregatedRect = std::move(*rectIter);
        m_aggregatedRects.erase(rectIter);

        QRegion leftover(aggregatedRect.rect);
        leftover -= newRect;

        for (const auto& subRect: leftover)
        {
            insertNonOverlappingRect(
                m_aggregatedRects.begin() + posToInsertAt,
                {subRect, aggregatedRect.values});
        }

        auto newValues = aggregatedRect.values;
        newValues.insert(newValue);

        return insertNonOverlappingRect(
            m_aggregatedRects.begin() + posToInsertAt,
            {newRect, std::move(newValues)});
    }

    /**
     * If there is an adjacent rect with same values, then merges adjacent rects instead of inserting.
     * NOTE: The caller MUST guarantee that data.rect does not intersect with any existing rect.
     * @return The iterator where data was inserted.
     */
    typename AggregatedRects::iterator insertNonOverlappingRect(
        typename AggregatedRects::iterator pos,
        const AggregatedRect& data)
    {
        // Checking for adjacent rect to merge to.
        for (auto it = m_aggregatedRects.begin(); it != m_aggregatedRects.end(); ++it)
        {
            if (it->values == data.values && adjacent(it->rect, data.rect))
            {
                it->rect = it->rect.united(data.rect);
                return it;
            }
        }

        return m_aggregatedRects.insert(pos, data);
    }

    bool adjacent(const QRect& one, const QRect& two)
    {
        const auto& united = one.united(two);
        return united.width() * united.height() ==
            one.width() * one.height() + two.width() * two.height();
    }

    void assertNoIntersectionWithAggregatedRects(
        const QRegion& region, int from = 0, int to = -1)
    {
        for (const auto& newRect: region)
            assertNoIntersectionWithAggregatedRects(newRect, from, to);
    }

    void assertNoIntersectionWithAggregatedRects(
        const QRect& rect, int from, int to)
    {
        if (to == -1)
            to = (int) m_aggregatedRects.size();

        for (std::size_t j = from; j < to; ++j)
        {
            NX_ASSERT(!m_aggregatedRects[j].rect.intersects(rect));
        }
    }
};
