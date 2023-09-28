// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "box.h"

#include <QtGui/QPolygon>

#include <nx/utils/log/assert.h>

#include "renderer.h"

namespace {

using nx::vms::client::desktop::figure::Points;

Points toBoxPoints(const Points& corners)
{
    if (!NX_ASSERT(corners.size() == 2, "Invalid source data for box figure"))
        return Points();

    const QRectF rect(corners.first(), corners.last());
    return {rect.topLeft(), rect.topRight(), rect.bottomRight(), rect.bottomLeft()};
}

} // namespace

namespace nx::vms::client::desktop::figure {

Box::Box(
    const Points& points,
    const QColor& color,
    const QRectF& sceneRect,
    core::StandardRotation sceneRotation)
    :
    base_type(FigureType::box, toBoxPoints(points), color, sceneRect, sceneRotation)
{
}

bool Box::isValid() const
{
    return pointsCount() == 4;
}

FigurePtr Box:: clone() const
{
    const auto boundingRect = QPolygonF(sourcePoints()).boundingRect();
    const Points boxPoints = {boundingRect.topLeft(), boundingRect.bottomRight()};
    return FigurePtr(new Box(boxPoints, color(), sceneRect(), sceneRotation()));
}

} // namespace nx::vms::client::desktop::figure
