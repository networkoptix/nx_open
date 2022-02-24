// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "bounded_blur_effect.h"

namespace nx::vms::client::desktop {

QRectF BoundedBlurEffect::boundingRectFor(const QRectF& rect) const
{
    // For some reason effective QRectF bounding blur effect always has even size values.
    // Odd size values reduced to lesser even values. Bounding rect 1px lesser than original size
    // produces worse appearance than 1px greater, so we add 0.5 margins to get a greater one.
    return rect.marginsAdded({0, 0, /*right*/ 0.5, /*bottom*/ 0.5});
}

} // nx::vms::client::desktop
