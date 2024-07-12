// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "atlas.h"

#include <algorithm>

namespace nx::pathkit {

namespace {

// Find a gap of at least requiredWidth. Returns the x coordinate of the gap and the index of the
// item to the right of the gap.
template<typename Container, typename T>
std::tuple<int, int> findGap(const Container &container, int T::*x, int T::*w, int requiredWidth)
{
    int gapX = 0;
    int i = 0;
    for (const auto& item: container)
    {
        if (item.*x - gapX >= requiredWidth)
            return {gapX, i};
        gapX = item.*x + item.*w;
        ++i;
    }
    return {gapX, i};
}

} // namespace

Atlas::Atlas(int width, int height): m_width(width), m_height(height)
{
}

Atlas::Rect Atlas::add(int w, int h)
{
    // Try to avoid wasting space by skipping the shelves that are >= 2*h.
    if (auto result = add(w, h, 2 * h); !result.isNull())
        return result;

    return add(w, h, m_height + 1);
}

bool Atlas::remove(int x, int y)
{
    auto shelfIt = std::find_if(m_shelves.begin(), m_shelves.end(),
        [y](auto s) { return y >= s.y && y < s.y + s.h; });
    if (shelfIt == m_shelves.end())
        return false;

    auto it = std::find_if(shelfIt->line.begin(), shelfIt->line.end(),
        [x](auto r) { return x >= r.x && x < r.x + r.w; });

    if (it == shelfIt->line.end())
        return false;

    shelfIt->line.erase(it);
    if (shelfIt->line.empty())
        m_shelves.erase(shelfIt);

    return true;
}

void Atlas::removeIf(std::function<bool(const Rect&)> pred)
{
    for (auto& shelf: m_shelves)
    {
        shelf.line.erase(
            std::remove_if(shelf.line.begin(), shelf.line.end(), pred),
            shelf.line.end());
    }

    m_shelves.erase(
        std::remove_if(
            m_shelves.begin(),
            m_shelves.end(),
            [](const auto& shelf) { return shelf.line.empty(); }),
        m_shelves.end());
}

Atlas::Rect Atlas::add(int w, int h, int hLimit)
{
    for (auto& shelf: m_shelves)
    {
        if (h <= shelf.h && hLimit > shelf.h)
        {
            auto [x, i] = findGap(shelf.line, &Rect::x, &Rect::w, w);
            if (x + w <= m_width)
            {
                Rect rect{.x = x, .y = shelf.y, .w = w, .h = h};
                shelf.line.insert(shelf.line.begin() + i, rect);
                return rect;
            }
        }
    }

    auto [y, i] = findGap(m_shelves, &Shelf::y, &Shelf::h, h);

    if (y + h > m_height)
        return {}; //< No space left.

    Shelf shelf{.y = y, .h = h};
    Rect rect{.x = 0, .y = y, .w = w, .h = h};
    shelf.line.push_back(rect);

    m_shelves.insert(m_shelves.begin() + i, std::move(shelf));

    return rect;
}

} // namespace nx::pathkit
