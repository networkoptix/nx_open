#pragma once

#include <set>

#include <QtCore/QRect>
#include <QtCore/QSize>

#include <nx/utils/log/log.h>

namespace nx::utils::math {

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
 * NOTE: Implemented by painting internal grid of predefined size.
 * So, all coordinates passed here must be within that predefined size.
 */
template<typename Value>
class RectAggregatorGrid
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

    using AggregatedRects = std::vector<AggregatedRect>;

    RectAggregatorGrid(const QSize& gridSize);

    void add(const QRect& rect, const Value& value);
    const std::vector<AggregatedRect>& aggregatedData() const;
    std::vector<AggregatedRect> takeAggregatedData();
    void clear();

private:
    using IntermediateAggregationResult =
        std::map<std::pair<int /*x*/, int /*color*/>, std::vector<QRect>>;

    static constexpr int kNoColor = 0;

    QSize m_gridSize;
    AggregatedRects m_aggregatedData;
    std::vector<std::vector<int /*color*/>> m_grid;
    std::map<std::set<Value>, int /*color*/> m_valueToColor;
    /**
     * Color is index.
     */
    std::vector<std::set<Value>> m_colorToValue;
    QRect m_boundingBox;

    void prepareAggregatedData();

    IntermediateAggregationResult readRectsFromGrid();

    void aggregateRects(IntermediateAggregationResult* rects);
};

//-------------------------------------------------------------------------------------------------

template<typename Value>
RectAggregatorGrid<Value>::RectAggregatorGrid(const QSize& gridSize):
    m_gridSize(gridSize)
{
    clear();
}

template<typename Value>
void RectAggregatorGrid<Value>::add(const QRect& rect, const Value& value)
{
    NX_ASSERT(rect.bottomRight().x() < m_gridSize.width()
        && rect.bottomRight().y() < m_gridSize.height());

    m_boundingBox = m_boundingBox.united(rect);

    for (int x = rect.x(); x <= rect.bottomRight().x(); ++x)
    {
        for (int y = rect.y(); y <= rect.bottomRight().y(); ++y)
        {
            int& currentColor = m_grid[x][y];
            const auto& currentValueSet = m_colorToValue[currentColor];
            if (currentValueSet.count(value) > 0)
            {
                // No need to change anything - the value is already contained in this cell.
                continue;
            }

            // Need to change the color of the cell.
            auto newValueSet = currentValueSet;
            newValueSet.insert(value);
            int& newColor = m_valueToColor[newValueSet];
            if (newColor == kNoColor)
            {
                m_colorToValue.push_back(std::move(newValueSet));
                newColor = (int) m_colorToValue.size() - 1;
            }

            currentColor = newColor;
        }
    }
}

template<typename Value>
const typename RectAggregatorGrid<Value>::AggregatedRects&
    RectAggregatorGrid<Value>::aggregatedData() const
{
    const_cast<RectAggregatorGrid*>(this)->prepareAggregatedData();
    return m_aggregatedData;
}

template<typename Value>
typename RectAggregatorGrid<Value>::AggregatedRects
    RectAggregatorGrid<Value>::takeAggregatedData()
{
    prepareAggregatedData();
    clear();
    return std::exchange(m_aggregatedData, {});
}

template<typename Value>
void RectAggregatorGrid<Value>::clear()
{
    m_grid.clear();

    m_grid.resize(m_gridSize.width());
    for (auto& column: m_grid)
        column.resize(m_gridSize.height());

    // Adding value of uinpainted cell (color kNoColor).
    m_colorToValue.clear();
    m_colorToValue.push_back(std::set<Value>());

    m_valueToColor.clear();

    m_boundingBox = QRect();
}

template<typename Value>
void RectAggregatorGrid<Value>::prepareAggregatedData()
{
    auto rects = readRectsFromGrid();

    aggregateRects(&rects);

    // Filling m_aggregatedData.
    for (auto& [xAndColor, rectSet]: rects)
    {
        const auto color = xAndColor.second;

        for (const auto& rect: rectSet)
        {
            m_aggregatedData.push_back(
                AggregatedRect{rect, m_colorToValue[color]});
        }
    }
}

template<typename Value>
typename RectAggregatorGrid<Value>::IntermediateAggregationResult
    RectAggregatorGrid<Value>::readRectsFromGrid()
{
    // Scanning line by line and saving one-line-high rects.

    std::map<std::pair<int /*x*/, int /*color*/>, std::vector<QRect>> rects;
    QRect* currentRect = nullptr;
    int prevColor = kNoColor;

    for (int y = m_boundingBox.y(); y <= m_boundingBox.bottomRight().y(); ++y)
    {
        for (int x = m_boundingBox.x(); x <= m_boundingBox.bottomRight().x(); ++x)
        {
            const auto color = m_grid[x][y];
            if (color != kNoColor)
            {
                if (color != prevColor)
                {
                    // Creating new rect.
                    auto& rectSet = rects[{x, color}];
                    rectSet.push_back(QRect(x, y, 1, 1));
                    currentRect = &rectSet.back();
                }
                else
                {
                    currentRect->setWidth(currentRect->width() + 1);
                }
            }

            prevColor = color;
        }

        currentRect = nullptr;
        prevColor = kNoColor;
    }

    return rects;
}

template<typename Value>
void RectAggregatorGrid<Value>::aggregateRects(
    IntermediateAggregationResult* rects)
{
    for (auto& [xAndColor, rectSet]: *rects)
    {
        // All rects in rectSet have same x.
        for (std::size_t i = 1; i < rectSet.size();)
        {
            if (rectSet[i - 1].width() == rectSet[i].width() &&
                rectSet[i - 1].y() + rectSet[i - 1].height() == rectSet[i].y())
            {
                // Merging rects.
                rectSet[i - 1].setHeight(rectSet[i - 1].height() + rectSet[i].height());
                rectSet.erase(rectSet.begin() + i);
            }
            else
            {
                ++i;
            }
        }
    }
}

} // namespace nx::utils::math
