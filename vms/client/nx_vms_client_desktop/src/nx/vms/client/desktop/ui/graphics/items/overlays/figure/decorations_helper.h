// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <optional>

#include <QtCore/QLineF>

#include "types.h"

namespace nx::vms::client::desktop::figure {

class Polyline;

struct LabelPosition
{
    QPointF coordinates{-1, -1};
    qreal angle = 0;
};

/*
 * Returns figure-specific position and angle of label text, if suitable position is exist.
 * in local coordinate system. Derived figures may return any visual rectangle,
 * even unrelated to the actual points.
 *
 * scale describes scene rect size.
 *
 * topMargin and bottomMargin describe space in pixels between label and figure side, in case label
 * is below edge and above edge respectively.
 */
std::optional<LabelPosition> getLabelPosition(
    const QSizeF& scale,
    FigurePtr figure,
    const QSizeF& labelSize,
    qreal topMargin,
    qreal bottomMargin);

/*
 * Returns poliline edge start point index. The edge is framed by triangle direction markers.
 * Edge end point index is "start index + 1".
 */
int directionMarksEdgeStartPointIndex(const Polyline* figure);

} // namespace nx::vms::client::desktop::figure
