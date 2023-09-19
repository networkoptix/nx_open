// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "polygon.h"

namespace nx::vms::client::desktop::figure {

Polygon::Polygon(
    const Points& points,
    const QColor& color,
    const QRectF& sceneRect,
    core::StandardRotation sceneRotation)
    :
    base_type(FigureType::polygon, points, color, sceneRect, sceneRotation)
{
}

Polygon::Polygon(
    FigureType type,
    const Points& points,
    const QColor& color,
    const QRectF& sceneRect,
    core::StandardRotation sceneRotation)
    :
    base_type(type, points, color, sceneRect, sceneRotation)
{
}

bool Polygon::isValid() const
{
    static constexpr int kMinimalPolygonPointsCount = 3;
    return pointsCount() >= kMinimalPolygonPointsCount;
}

FigurePtr Polygon::clone() const
{
    return FigurePtr(new Polygon(sourcePoints(), color(), sceneRect(), sceneRotation()));
}

} // namespace nx::vms::client::desktop::figure
