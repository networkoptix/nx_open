// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "figure.h"

namespace nx::vms::client::desktop::figure {

/**
 * Represents figure with a open shape, like a point or line.
 * Reimplements containsPoint check and returns if visual rectangle for the
 * figure contains some point.
 */
class OpenShapeFigure: public Figure
{
    using base_type = Figure;

public:
    virtual bool containsPoint(
        const Points::value_type& point,
        const QSizeF& scale) const override;

protected:
    OpenShapeFigure(
        FigureType type,
        const Points& points,
        const QColor& color,
        const QRectF& sceneRect,
        core::StandardRotation sceneRotation);
};

} // namespace nx::vms::client::desktop::figure
