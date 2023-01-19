// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/sdk/analytics/point.h>

namespace nx::sdk::analytics {

struct Rect
{
    /** Creates an invalid rectangle. */
    Rect() = default;

    Rect(float x, float y, float width, float height): x(x), y(y), width(width), height(height) {}

    Rect(Point topLeft, float width, float height):
        x(topLeft.x), y(topLeft.y), width(width), height(height)
    {
    }

    /**
     * X coordinate of the top left corner. Valid value must be in the range [0..1]. Zero is at the
     * leftmost position in the video frame.
     */
    float x = -1;

    /**
     * Y coordinate of the top left corner. Valid value must be in the range [0..1]. Zero is at the
     * topmost position in the video frame.
     */
    float y = -1;

    /**
     * Width of the rectangle. Valid value must be in the range [0..1], and the rule
     * `x + width <= 1` must be satisfied.
     */
    float width = -1;

    /**
     * Height of the rectangle. Valid value must be in the range [0..1], and the rule
     * `y + height <= 1` must be satisfied.
     */
    float height = -1;

    /**
     * Calculates the center of the rectangle.
     *
     * @return Point with both coordinates in the range [0..1].
     */
    Point center() const { return Point(x + width / 2, y + height / 2); }

    bool isValid() const
    {
        return x >= 0 && y >= 0 && width >= 0 && height >= 0
            && x + width <= 1 && y + height <= 1;
    }
};

} // namespace nx::sdk::analytics
