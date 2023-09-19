// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "open_shape_figure.h"

namespace nx::vms::client::desktop::figure {

/** Represents polyline figure. */
class Polyline: public OpenShapeFigure
{
    using base_type = OpenShapeFigure;

public:
    static const qreal kDirectionMarkWidth;
    static const qreal kDirectionMarkHeight;
    static const qreal kDirectionMarkOffset;

    enum class Direction
    {
        both,
        left,
        right,
        none
    };

    Polyline(
        const Points& points,
        const QColor& color,
        Direction direction,
        const QRectF& sceneRect = QRectF(),
        core::StandardRotation sceneRotation = core::StandardRotation::rotate0);

    Direction direction() const;

    virtual QRectF visualRect(const QSizeF& scale) const override;
    virtual bool isValid() const override;
    virtual bool equalsTo(const Figure& right) const override;
    virtual FigurePtr clone() const override;

private:
    Direction m_direction;
};

} // namespace nx::vms::client::desktop::figure
