#pragma once

#include <set>

#include <QtCore/QRect>
#include <QtGui/QRegion>

#include <nx/utils/log/assert.h>

namespace nx::analytics::db {

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
        QRegion region(rect);

        for (auto it = m_aggregatedRects.begin();
            it != m_aggregatedRects.end();
            ++it)
        {
            const auto& aggregatedRect = *it;
            if (!rect.intersects(aggregatedRect.rect))
                continue;

            const auto intersectionRegion = region.intersected(aggregatedRect.rect);
            if (intersectionRegion.isEmpty())
                continue;

            // It is always so since m_aggregatedRects does not contain intersecting rects.
            NX_ASSERT(intersectionRegion.rectCount() == 1);

            const auto intersection = *intersectionRegion.begin();
            if (aggregatedRect.values.find(value) == aggregatedRect.values.end())
            {
                if (intersection != aggregatedRect.rect)
                    it = splitRectInplace(it, intersection, value);
                it->values.insert(value);
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
        return std::exchange(m_aggregatedRects, {});
    }

    void clear()
    {
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
     * @return Iterator pointing to the rect equal to requiredSubRect.
     */
    typename AggregatedRects::iterator splitRectInplace(
        typename AggregatedRects::iterator rectIter,
        const QRect& requiredSubRect,
        const Value& value)
    {
        NX_ASSERT(rectIter->rect.contains(requiredSubRect));

        const auto posToInsertAt = rectIter - m_aggregatedRects.begin();
        const auto aggregatedRect = std::move(*rectIter);
        m_aggregatedRects.erase(rectIter);

        auto leftover = QRegion(aggregatedRect.rect) - requiredSubRect;

        for (const auto& subRect: leftover)
        {
            insertNonOverlappingRect(
                m_aggregatedRects.begin() + posToInsertAt,
                {subRect, aggregatedRect.values});
        }

        auto newValues = aggregatedRect.values;
        newValues.insert(value);

        return insertNonOverlappingRect(
            m_aggregatedRects.begin() + posToInsertAt,
            {requiredSubRect, std::move(newValues)});
    }

    /**
     * If there is adjacent rect with same values, then merges adjacent rects instead of inserting.
     * NOTE: The caller MUST guarantee that data.rect does not intersect with any existing rect.
     */
    typename AggregatedRects::iterator insertNonOverlappingRect(
        typename AggregatedRects::iterator pos,
        const AggregatedRect& data)
    {
        // Checking for adjasent rect to merge to.
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
};

} // namespace nx::analytics::db
