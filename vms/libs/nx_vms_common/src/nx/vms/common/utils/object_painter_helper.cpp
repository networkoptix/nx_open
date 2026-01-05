// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "object_painter_helper.h"

namespace nx::vms::common {

bool ObjectPainterHelper::hasHorizontalSpace(const QMarginsF& space)
{
    return space.left() > kMinimalTooltipOffset && space.right() > kMinimalTooltipOffset;
}

bool ObjectPainterHelper::hasVerticalSpace(const QMarginsF& space)
{
    return space.top() > kMinimalTooltipOffset && space.bottom() > kMinimalTooltipOffset;
}

ObjectPainterHelper::BestSide ObjectPainterHelper::findBestSide(
    const QSizeF& labelSize,
    const QMarginsF& space)
{
    if (space.bottom() - kMinimalTooltipOffset >= labelSize.height() && hasHorizontalSpace(space))
        return {Qt::BottomEdge, /*proportion*/ 1};
    else if (
        space.top() - kMinimalTooltipOffset >= labelSize.height() && hasHorizontalSpace(space))
        return {Qt::TopEdge, /*proportion*/ 1};
    else if (space.left() - kMinimalTooltipOffset >= labelSize.width() && hasVerticalSpace(space))
        return {Qt::LeftEdge, /*proportion*/ 1};
    else if (space.right() - kMinimalTooltipOffset >= labelSize.width() && hasVerticalSpace(space))
        return {Qt::RightEdge, /*proportion*/ 1};

    BestSide result;
    auto checkSide = [&result](Qt::Edge side, qreal labelSize, qreal space)
    {
        const qreal proportion = space / labelSize;
        if (proportion >= 1.0 || proportion >= result.proportion)
        {
            result.proportion = proportion;
            result.edge = side;
        }
    };

    checkSide(Qt::LeftEdge, labelSize.width(), space.left());
    checkSide(Qt::RightEdge, labelSize.width(), space.right());
    checkSide(Qt::TopEdge, labelSize.height(), space.top());
    checkSide(Qt::BottomEdge, labelSize.height(), space.bottom());
    return result;
}

QRectF ObjectPainterHelper::calculateLabelGeometry(
    QRectF boundingRect,
    QSizeF labelSize,
    QMarginsF labelMargins,
    QRectF objectRect)
{
    constexpr qreal kTitleAreaHeight = 24;
    boundingRect = boundingRect.adjusted(0, kTitleAreaHeight, 0, 0);
    labelSize = Geometry::bounded(labelSize, boundingRect.size());

    const QMarginsF space(
        objectRect.left() - boundingRect.left(),
        objectRect.top() - boundingRect.top(),
        boundingRect.right() - objectRect.right(),
        boundingRect.bottom() - objectRect.bottom());

    BestSide bestSide = findBestSide(labelSize, space);

    static constexpr qreal kLabelPadding = 2.0;
    QRectF availableRect = Geometry::dilated(boundingRect.intersected(objectRect), kLabelPadding);

    if (bestSide.proportion < 1.0
        && availableRect.width() / labelSize.width() > bestSide.proportion
        && availableRect.height() / labelSize.height() > bestSide.proportion)
    {
        labelSize = Geometry::bounded(labelSize, availableRect.size());
        return Geometry::aligned(labelSize, availableRect, Qt::AlignTop | Qt::AlignRight);
    }

    static constexpr qreal kArrowBase = 6;
    QPointF pos;
    switch (bestSide.edge)
    {
        case Qt::LeftEdge:
            pos = QPointF(
                objectRect.left() - labelSize.width(),
                objectRect.top() - labelMargins.top() - kArrowBase);
            availableRect = QRectF(
                boundingRect.left(),
                boundingRect.top(),
                space.left(),
                boundingRect.height());
            break;
        case Qt::RightEdge:
            pos = QPointF(
                objectRect.right(),
                objectRect.top() - labelMargins.top() - kArrowBase);
            availableRect = QRectF(
                objectRect.right(),
                boundingRect.top(),
                space.right(),
                boundingRect.height());
            break;
        case Qt::TopEdge:
            pos = QPointF(
                objectRect.left() - labelMargins.left() - kArrowBase,
                objectRect.top() - labelSize.height());
            availableRect = QRectF(
                boundingRect.left(),
                boundingRect.top(),
                boundingRect.width(),
                space.top());
            break;
        case Qt::BottomEdge:
            pos = QPointF(
                objectRect.left() - labelMargins.left() - kArrowBase,
                objectRect.bottom());
            availableRect = QRectF(
                boundingRect.left(),
                std::max(objectRect.bottom(), boundingRect.top()),
                boundingRect.width(),
                space.bottom());
            break;
    }

    labelSize = Geometry::bounded(labelSize, availableRect.size());
    return Geometry::movedInto(QRectF(pos, labelSize), availableRect);
}

QColor ObjectPainterHelper::calculateTooltipColor(const QColor& frameColor)
{
    static constexpr int kTooltipBackgroundLightness = 10;
    static constexpr int kTooltipBackgroundAlpha = 127;

    auto color = frameColor.toHsl();
    color = QColor::fromHsl(
        color.hslHue(),
        color.hslSaturation(),
        kTooltipBackgroundLightness,
        kTooltipBackgroundAlpha);
    return color;
}

} // namespace nx::vms::common
