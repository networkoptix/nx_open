// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "polygon.h"

namespace nx::vms::client::desktop::figure {

/** Represents box figure. */
class Box: public Polygon
{
    using base_type = Polygon;

public:
    /**
     * Creates box figure.
     * @param points Should contain top left and bottom right values ony.
     */
    Box(
        const Points& points,
        const QColor& color,
        const QRectF& sceneRect = QRectF(),
        core::StandardRotation sceneRotation = core::StandardRotation::rotate0);

    virtual bool isValid() const override;
    virtual FigurePtr clone() const override;
};

} // namespace nx::vms::client::desktop::figure
