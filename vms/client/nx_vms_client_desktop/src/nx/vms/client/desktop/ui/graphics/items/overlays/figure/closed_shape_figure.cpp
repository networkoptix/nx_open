// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "closed_shape_figure.h"

#include <QtGui/QPolygon>

namespace nx::vms::client::desktop::figure {

bool ClosedShapeFigure::containsPoint(
    const Points::value_type& point,
    const QSizeF& scale) const
{
    static constexpr int kMinimalPointsInPolygon = 3;
    const auto& absolutePoints = points(scale);
    const auto polygon = absolutePoints.size() >= kMinimalPointsInPolygon
        ? QPolygonF(absolutePoints)
        : QPolygonF(visualRect(scale));

    return polygon.containsPoint(point, Qt::OddEvenFill);
}

ClosedShapeFigure::ClosedShapeFigure(
    FigureType type,
    const Points& points,
    const QColor& color,
    const QRectF& sceneRect,
    core::StandardRotation sceneRotation)
    :
    base_type(type, points, color, sceneRect, sceneRotation)
{
}

} // namespace nx::vms::client::desktop::figure
