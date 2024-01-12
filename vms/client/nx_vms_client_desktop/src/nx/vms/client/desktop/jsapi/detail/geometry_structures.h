// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/reflect/instrument.h>

namespace nx::vms::client::desktop::jsapi::detail {

/**
 * @ingroup vms
 */
struct Point
{
    int x = 0;
    int y = 0;
};
NX_REFLECTION_INSTRUMENT(Point, (x)(y))

/**
 * @ingroup vms
 */
struct Size
{
    int width = 0;
    int height = 0;
};
NX_REFLECTION_INSTRUMENT(Size, (width)(height))

/**
 * @ingroup vms
 */
struct Rect
{
    /** Top left point of the rectangle. */
    Point pos;
    Size size;

    /** @private */
    template<typename Source>
    static Rect from(const Source& source)
    {
        return Rect{
            Point{static_cast<int>(source.left()), static_cast<int>(source.top())},
            Size{static_cast<int>(source.width()), static_cast<int>(source.height())}};
    }
};
NX_REFLECTION_INSTRUMENT(Rect, (pos)(size))

} // namespace nx::vms::client::desktop::jsapi::detail
