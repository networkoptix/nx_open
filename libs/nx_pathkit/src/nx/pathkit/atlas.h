// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <functional>
#include <vector>

namespace nx::pathkit {

// 2D texture atlas using simple shelf allocator, see https://nical.github.io/posts/etagere.html
class NX_PATHKIT_API Atlas
{
public:
    struct Rect
    {
        int x = 0;
        int y = 0;
        int w = 0;
        int h = 0;

        bool operator==(const Rect&) const = default;
        bool isNull() const { return x <= 0 && y <= 0 && w <= 0 && h <= 0; }
    };

private:
    struct Shelf
    {
        int y = 0;
        int h = 0;
        std::vector<Rect> line;
    };

public:
    Atlas(int width, int height);

    Rect add(int w, int h);

    bool remove(const Rect& r) { return remove(r.x, r.y); }
    bool remove(int x, int y);
    void removeIf(std::function<bool(const Rect&)> pred);

    int width() const { return m_width; }
    int height() const { return m_height; }

    void clear() { m_shelves.clear(); }

private:
    Rect add(int w, int h, int hLimit);

    int m_width;
    int m_height;
    std::vector<Shelf> m_shelves;
};

} // namespace nx::pathkit

template <>
struct std::hash<nx::pathkit::Atlas::Rect>
{
    std::size_t operator()(const nx::pathkit::Atlas::Rect& r) const noexcept
    {
        return (r.x << 16) + r.y;
    }
};
