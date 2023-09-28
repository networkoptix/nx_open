// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "closed_shape_figure.h"

namespace nx::vms::client::desktop::figure {

/** Represents polygon figure. */
class Polygon: public ClosedShapeFigure
{
    using base_type = ClosedShapeFigure;

public:
    Polygon(
        const Points& points,
        const QColor& color,
        const QRectF& sceneRect = QRectF(),
        core::StandardRotation sceneRotation = core::StandardRotation::rotate0);

    virtual bool isValid() const override;
    virtual FigurePtr clone() const override;

protected:
    /** Contructor for derived types, like box, etc.*/
    Polygon(
        FigureType type,
        const Points& points,
        const QColor& color,
        const QRectF& sceneRect,
        core::StandardRotation sceneRotation);
};

} // namespace nx::vms::client::desktop::figure
