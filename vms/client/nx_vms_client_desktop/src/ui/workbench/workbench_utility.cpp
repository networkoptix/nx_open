// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "workbench_utility.h"

#include <cmath>

#include <nx/vms/client/core/utils/geometry.h>

using nx::vms::client::core::Geometry;

QnDistanceMagnitudeCalculator::QnDistanceMagnitudeCalculator(const QPointF& origin):
    m_origin(origin),
    m_calculator(MagnitudeCalculator::forType<QPointF>())
{
}

qreal QnDistanceMagnitudeCalculator::calculateInternal(const void* value) const
{
    const auto& p = *static_cast<const QPoint*>(value);
    return m_calculator->calculate(p - m_origin);
}

QnAspectRatioMagnitudeCalculator::QnAspectRatioMagnitudeCalculator(
    const QPointF& origin,
    const QSize& size,
    const QRect& boundary,
    qreal aspectRatio)
    :
    m_origin(origin),
    m_size(Geometry::toPoint(size)),
    m_boundary(boundary),
    m_aspectRatio(aspectRatio)
{
}

qreal QnAspectRatioMagnitudeCalculator::calculateInternal(const void* value) const
{
    const QPoint& p = *static_cast<const QPoint*>(value);

    QPointF delta = p + QPointF(m_size) / 2.0 - m_origin;
    qreal spaceDistance = qMax(qAbs(delta.x() / m_aspectRatio), qAbs(delta.y()));

    QRect extendedRect = QRect(
        QPoint(
            qMin(m_boundary.left(), p.x() - m_size.x() + 1),
            qMin(m_boundary.top(), p.y() - m_size.y() + 1)),
        QPoint(
            qMax(m_boundary.right(), p.x() + m_size.x() - 1),
            qMax(m_boundary.bottom(), p.y() + m_size.y() - 1)));

    qreal aspectDistance = qAbs(std::log(Geometry::aspectRatio(extendedRect) / m_aspectRatio));

    qreal extensionDistance = qAbs(m_boundary.width() - extendedRect.width())
        + qAbs(m_boundary.height() - extendedRect.height());

    qreal k = m_boundary.width() + m_boundary.height();
    return k * k * extensionDistance + k * aspectDistance + spaceDistance;
}

QSize bestSingleBoundedSize(
    QnWorkbenchGridMapper* mapper,
    int bound,
    Qt::Orientation boundOrientation,
    qreal aspectRatio)
{
    QSizeF sceneSize = mapper->mapFromGrid(
        boundOrientation == Qt::Horizontal ? QSize(bound, 0) : QSize(0, bound));

    if (boundOrientation == Qt::Horizontal)
        sceneSize.setHeight(sceneSize.width() / aspectRatio);
    else
        sceneSize.setWidth(sceneSize.height() * aspectRatio);

    QSize gridSize0 = mapper->mapToGrid(sceneSize);
    QSize gridSize1 = gridSize0;
    if (boundOrientation == Qt::Horizontal)
        gridSize1.setHeight(qMax(gridSize1.height() - 1, 1));
    else
        gridSize1.setWidth(qMax(gridSize1.width() - 1, 1));

    QSize gridSize2 = gridSize1;
    if (boundOrientation == Qt::Horizontal)
        gridSize2.setWidth(qMax(gridSize2.width() - 1, 1));
    else
        gridSize2.setHeight(qMax(gridSize2.height() - 1, 1));

    qreal aspectRatio1 =
        Geometry::aspectRatio(mapper->mapFromGrid(gridSize1));
    qreal aspectRatio2 =
        Geometry::aspectRatio(mapper->mapFromGrid(gridSize2));

    if (gridSize0.isEmpty())
        gridSize0 = gridSize0.expandedTo(QSize(1, 1));

    if (gridSize1.isEmpty())
        gridSize1 = gridSize1.expandedTo(QSize(1, 1));

    if ((aspectRatio1 < aspectRatio) == (aspectRatio2 < aspectRatio) && gridSize1 != gridSize2)
        return gridSize0;

    qreal distance0 = std::abs(std::log(
        Geometry::aspectRatio(
            mapper->mapFromGrid(gridSize0)) / aspectRatio)) / 1.25; //< Prefer larger size.
    qreal distance1 = std::abs(
        std::log(Geometry::aspectRatio(mapper->mapFromGrid(gridSize1)) / aspectRatio));

    return distance0 < distance1 ? gridSize0 : gridSize1;
}

QSize bestDoubleBoundedSize(QnWorkbenchGridMapper* mapper, const QSize& bound, qreal aspectRatio)
{
    qreal boundAspectRatio = Geometry::aspectRatio(mapper->mapFromGrid(bound));

    if (aspectRatio < boundAspectRatio)
        return bestSingleBoundedSize(mapper, bound.height(), Qt::Vertical, aspectRatio);
    else
        return bestSingleBoundedSize(mapper, bound.width(), Qt::Horizontal, aspectRatio);
}

QRect bestDoubleBoundedGeometry(
    QnWorkbenchGridMapper* mapper, const QRect& bound, qreal aspectRatio)
{
    QSize size = bestDoubleBoundedSize(mapper, bound.size(), aspectRatio);
    return QRect(bound.topLeft() + Geometry::toPoint(bound.size() - size) / 2, size);
}
