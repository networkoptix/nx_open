// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "figure.h"

namespace nx::vms::client::desktop::figure {

/**
 * Represents figure with a closed shape, like a box or polygon.
 * Reimplements containsPoint check keeping in mind that figure shape type.
 */
class ClosedShapeFigure: public Figure
{
    using base_type = Figure;

public:
    virtual bool containsPoint(
        const Points::value_type& point,
        const QSizeF& scale) const;

protected:
    ClosedShapeFigure(
        FigureType type,
        const Points& points,
        const QColor& color,
        const QRectF& sceneRect,
        core::StandardRotation sceneRotation);
};

} // namespace nx::vms::client::desktop::figure
