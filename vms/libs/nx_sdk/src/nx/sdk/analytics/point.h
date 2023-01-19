// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

namespace nx::sdk::analytics {

struct Point
{
    Point() = default;

    Point(float x, float y): x(x), y(y) {}

    /**
     * X coordinate. Valid value must be in the range [0..1].
     */
    float x = -1;

    /**
     * Y coordinate. Valid value must be in the range [0..1].
     */
    float y = -1;
};

} // namespace nx::sdk::analytics
