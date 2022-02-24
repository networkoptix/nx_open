// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "open_shape_figure.h"

#include <QtGui/QPolygon>

namespace nx::vms::client::desktop::figure {

bool OpenShapeFigure::containsPoint(
    const Points::value_type& point,
    const QSizeF& scale) const
{
    return QPolygonF(visualRect(scale)).containsPoint(point, Qt::OddEvenFill);
}

OpenShapeFigure::OpenShapeFigure(
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
