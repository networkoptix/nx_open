// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "tab_width_calculator.h"

#include <nx/utils/log/assert.h>

void TabWidthCalculator::calculate(int desiredWidth)
{
    reset();
    if (m_orderedTabWidths.isEmpty())
        return;

    int overflow = m_sumTabWidths - desiredWidth;
    auto orderedTabWidths = m_orderedTabWidths;
    int reminder = 0;
    while (overflow > 0)
    {
        auto last = orderedTabWidths.rbegin();
        const auto preceding = last + 1;
        if (preceding == orderedTabWidths.rend())
        {
            orderedTabWidths[0].width = desiredWidth / orderedTabWidths[0].count;
            reminder = desiredWidth % orderedTabWidths[0].count;
            break;
        }
        const int diff = (last->width - preceding->width) * last->count;
        if (overflow > diff)
        {
            preceding->count += last->count;
            orderedTabWidths.removeLast();
            overflow -= diff;
        }
        else
        {
            const int maxWidthTabs = last->count;
            const int available = last->width * maxWidthTabs - overflow;
            last->width = available / maxWidthTabs;
            reminder = available % maxWidthTabs;
            break;
        }
    }

    const int maxWidth = orderedTabWidths.last().width;
    for (int i = 0; i < m_tabWidths.size(); ++i)
    {
        if (m_tabWidths[i] > maxWidth)
            m_tabWidths[i] = maxWidth + (reminder-- > 0 ? 1 : 0);
    }
}

void TabWidthCalculator::clear()
{
    m_tabOriginalWidths.clear();
    m_orderedTabWidths.clear();
    m_sumTabWidths = 0;
}

void TabWidthCalculator::addTab(int width)
{
    m_tabOriginalWidths.append(width);
    m_sumTabWidths += width;
    addOrderedWidth(width);
}

void TabWidthCalculator::setWidth(int index, int width)
{
    NX_ASSERT(index < m_tabOriginalWidths.size());
    int previousWidth = m_tabOriginalWidths[index];
    if (width == previousWidth)
        return;

    m_sumTabWidths = m_sumTabWidths - previousWidth + width;
    m_tabOriginalWidths[index] = width;

    auto it = findOrderedBound(previousWidth);
    NX_ASSERT(it != m_orderedTabWidths.end());
    if (it->count == 1)
        m_orderedTabWidths.erase(it);
    else
        it->count--;

    addOrderedWidth(width);
}

void TabWidthCalculator::moveTab(int from, int to)
{
    m_tabOriginalWidths.move(from, to);
    m_tabWidths.move(from, to);
}

void TabWidthCalculator::reset()
{
    m_tabWidths = m_tabOriginalWidths;
}

int TabWidthCalculator::count() const
{
    return m_tabWidths.size();
}

int TabWidthCalculator::width(int index) const
{
    return m_tabWidths[index];
}

QList<TabWidthCalculator::WidthInfo>::iterator TabWidthCalculator::findOrderedBound(int width)
{
    const auto it = std::ranges::lower_bound(
        m_orderedTabWidths,
        WidthInfo{.width = width},
        [](const WidthInfo& left, const WidthInfo& right)
        {
            return left.width < right.width;
        });
    return it;
}

void TabWidthCalculator::addOrderedWidth(int width)
{
    const auto it = findOrderedBound(width);
    if (it == m_orderedTabWidths.end() || it->width > width)
        m_orderedTabWidths.insert(it, {.width = width, .count = 1});
    else
        it->count++;
}
