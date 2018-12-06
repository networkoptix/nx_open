#pragma once

#include "workbench_grid_mapper.h"
#include <utils/math/magnitude.h>

/**
 * Simple magnitude calculator for finding the best place for an item.
 */
class QnDistanceMagnitudeCalculator: public TypedMagnitudeCalculator<QPoint>
{
public:
    /**
     * @param origin Desired position for an item.
     */
    QnDistanceMagnitudeCalculator(const QPointF& origin);

protected:
    virtual qreal calculateInternal(const void* value) const override;

protected:
    QPointF m_origin;
    TypedMagnitudeCalculator<QPointF>* m_calculator;
};

/**
 * Magnitude calculator for finding the best place for an item.
 *
 * Takes viewport's aspect ratio and scene bounding rect into account.
 */
class QnAspectRatioMagnitudeCalculator: public TypedMagnitudeCalculator<QPoint>
{
public:
    /**
     * @param origin Desired position for an item.
     * @param size Item size.
     * @param boundary Scene bounding rect. Magnitude calculator will assign higher weights for
     *     positions outside the bounding rect.
     * @param aspectRatio Aspect ratio of the viewport, in item coordinate system.
     */
    QnAspectRatioMagnitudeCalculator(
        const QPointF& origin,
        const QSize& size,
        const QRect& boundary,
        qreal aspectRatio);

protected:
    virtual qreal calculateInternal(const void* value) const override;

private:
    QPointF m_origin;
    QPoint m_size;
    QRect m_boundary;
    qreal m_aspectRatio;
};

QSize bestSingleBoundedSize(
    QnWorkbenchGridMapper* mapper,
    int bound,
    Qt::Orientation boundOrientation,
    qreal aspectRatio);

QSize bestDoubleBoundedSize(
    QnWorkbenchGridMapper* mapper,
    const QSize& bound,
    qreal aspectRatio);

QRect bestDoubleBoundedGeometry(
    QnWorkbenchGridMapper* mapper,
    const QRect& bound,
    qreal aspectRatio);
