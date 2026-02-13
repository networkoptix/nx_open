// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QPointF>
#include <QtCore/QRectF>
#include <QtGui/QPainter>

#include <nx/media/meta_data_packet.h>

namespace nx::vms::common {

constexpr qreal kMinimalTooltipOffset = 16;

struct NX_VMS_COMMON_API ObjectPainterHelper
{
    struct BestSide
    {
        Qt::Edge edge = static_cast<Qt::Edge>(0);
        qreal proportion = 0;
    };

    static bool hasHorizontalSpace(const QMarginsF& space);
    static bool hasVerticalSpace(const QMarginsF& space);

    /** Tries to find the appropriate side to which place the tooltip. */
    static BestSide findBestSide(const QSizeF& labelSize, const QMarginsF& space);

    static QRectF calculateLabelGeometry(
        QRectF boundingRect,
        QSizeF labelSize,
        QMarginsF labelMargins,
        QRectF objectRect);

    static QColor calculateTooltipColor(const QColor& frameColor);
    static QList<QList<QPointF>> calculateMotionGrid(
        const QRectF& rect,
        const QnConstMetaDataV1Ptr& motion);
};

} // namespace nx::vms::common
