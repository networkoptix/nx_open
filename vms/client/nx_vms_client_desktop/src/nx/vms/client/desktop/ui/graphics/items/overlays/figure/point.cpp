// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "point.h"

namespace {

static const qreal kSideSize = 18;
static const QRectF kBoundingRect(
    QPointF(-kSideSize / 2, -kSideSize / 2),
    QSizeF(kSideSize, kSideSize));

} // namespace

namespace nx::vms::client::desktop::figure {

Point::Point(
    const Points::value_type& point,
    const QColor& color,
    const QRectF& targetRect,
    core::StandardRotation rotation)
    :
    base_type(FigureType::point, {point}, color, targetRect, rotation)
{
}

QRectF Point::visualRect(const QSizeF& /*scale*/) const
{
    return kBoundingRect;
}

bool Point::isValid() const
{
    return pointsCount() == 1;
}

FigurePtr Point::clone() const
{
    return FigurePtr(new Point(sourcePoints().first(), color()));
}

} // namespace nx::vms::client::desktop::figure
