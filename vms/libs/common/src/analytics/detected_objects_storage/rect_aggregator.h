#pragma once

namespace nx::analytics::storage {

/**
 * Aggregates / splits provided rectangles so that the result does not contain
 * interlapping rectangles while preserving association between area and value.
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
 * Example 3 (edge case: output is larger than input):
 * Input: {r{1;1;5;5}, v1}, {r{2;2;4;4}, v2}.
 * Output: {r{1;1;5;2}, v1}, {r{4;1;5;5}, v1}, {r{1;4;5;5}, v1}, {r{1;1;2;5}, v1}, {r{2;2;4;4}, {v1, v2}}.
 */
template<typename Value>
class RectAggregator
{
public:
    struct AggregatedRect
    {
        QRect rect;
        std::vector<Value> values;
    };

    void add(const QRect& /*rect*/, const Value& /*value*/)
    {
        // TODO
    }

    const std::vector<AggregatedRect>& aggregatedData() const
    {
        // TODO
        return m_aggregatedData;
    }

    void clear()
    {
        // TODO
    }

    /**
     * @return Number of produced rectangles so far.
     */
    std::size_t size() const
    {
        return m_aggregatedData.size();
    }

private:
    std::vector<AggregatedRect> m_aggregatedData;
};

} // namespace nx::analytics::storage
