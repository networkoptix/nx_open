// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "polyline.h"

namespace nx::vms::client::desktop::figure {

const qreal Polyline::kDirectionMarkWidth = 16;
const qreal Polyline::kDirectionMarkHeight = 8;
const qreal Polyline::kDirectionMarkOffset = 3;

Polyline::Polyline(
    const Points& points,
    const QColor& color,
    Direction direction,
    const QRectF& sceneRect,
    core::StandardRotation sceneRotation)
    :
    base_type(FigureType::polyline, points, color, sceneRect, sceneRotation),
    m_direction(direction)
{
}

Polyline::Direction Polyline::direction() const
{
    return m_direction;
}

QRectF Polyline::visualRect(const QSizeF& scale) const
{
    QRectF result = base_type::visualRect(scale);

    if (m_direction == Direction::none)
    {
        if (result.width() == 0)
            result.setWidth(1);
        if (result.height() == 0)
            result.setHeight(1);

        return result;
    }

    // If code execution is here, there are direction marker arrows around the line,
    // they are drawn inside renderer.
    // Let's increase visual rect to provide space onto painter for arrows,
    // to prevent clipping by bounding rect.
    // Choose max from two dimensions to guarantee enough space for any line's tilt and size.
    // It's a rough approximation instead of time consuming calculating.
    static const qreal kMinSideSize = (kDirectionMarkHeight + kDirectionMarkOffset) * 2;
    if (result.width() < kMinSideSize)
    {
        result.moveLeft(-kMinSideSize / 2);
        result.setWidth(kMinSideSize);
    }
    if (result.height() < kMinSideSize)
    {
        result.moveTop(-kMinSideSize / 2);
        result.setHeight(kMinSideSize);
    }

    return result;
}

bool Polyline::isValid() const
{
    static constexpr int kMinimalLinePointsCount = 2;
    return pointsCount() >= kMinimalLinePointsCount;
}

bool Polyline::equalsTo(const Figure& right) const
{
    return base_type::equalsTo(right)
        && direction() == static_cast<const Polyline&>(right).direction();
}

FigurePtr Polyline::clone() const
{
    return FigurePtr(new Polyline(sourcePoints(), color(), direction(), sceneRect(), sceneRotation()));
}

} // namespace nx::vms::client::desktop::figure
