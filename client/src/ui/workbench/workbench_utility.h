#ifndef QN_WORKBENCH_UTILITY_H
#define QN_WORKBENCH_UTILITY_H

#include <ui/common/scene_utility.h>
#include <ui/common/magnitude.h>
#include <cmath>
#include "workbench_grid_mapper.h"

/**
 * Simple magnitude calculator for finding the best place for an item. 
 */
class QnDistanceMagnitudeCalculator: public TypedMagnitudeCalculator<QPoint> {
public:
    QnDistanceMagnitudeCalculator(const QPointF &origin):
        m_origin(origin),
        m_calculator(MagnitudeCalculator::forType<QPointF>())
    {}

protected:
    virtual qreal calculateInternal(const void *value) const override {
        const QPoint &p = *static_cast<const QPoint *>(value);

        return m_calculator->calculate(p - m_origin);
    }

private:
    QPointF m_origin;
    TypedMagnitudeCalculator<QPointF> *m_calculator;
};


/**
 * Magnitude calculator for finding the best place for an item. 
 * 
 * Takes viewport's aspect ratio and scene bounding rect into account.
 */
class QnAspectRatioMagnitudeCalculator: public TypedMagnitudeCalculator<QPoint> {
public:
    QnAspectRatioMagnitudeCalculator(const QPointF &origin, const QSize &size, const QRect &boundary, qreal aspectRatio):
        m_origin(origin),
        m_size(SceneUtility::toPoint(size)),
        m_boundary(boundary),
        m_aspectRatio(aspectRatio)
    {}

protected:
    virtual qreal calculateInternal(const void *value) const override {
        const QPoint &p = *static_cast<const QPoint *>(value);

        QPointF delta = p + QPointF(m_size) / 2.0 - m_origin;
        qreal spaceDistance = qMax(qAbs(delta.x() / m_aspectRatio), qAbs(delta.y()));

        QRect extendedRect = QRect(
            QPoint(
                qMin(m_boundary.left(), p.x()),
                qMin(m_boundary.top(), p.y())
            ),
            QPoint(
                qMax(m_boundary.right(), p.x() + m_size.x() - 1),
                qMax(m_boundary.bottom(), p.y() + m_size.y() - 1)
            )
        );
        qreal aspectDistance = qAbs(std::log(SceneUtility::aspectRatio(extendedRect) / m_aspectRatio));

        qreal extensionDistance = qAbs(m_boundary.width() - extendedRect.width()) + qAbs(m_boundary.height() - extendedRect.height());

        qreal k = (m_boundary.width() + m_boundary.height());
        return k * k * extensionDistance + k * aspectDistance + spaceDistance;
    }

private:
    QPointF m_origin;
    QPoint m_size;
    QRect m_boundary;
    qreal m_aspectRatio;
};


inline QSize bestSingleBoundedSize(QnWorkbenchGridMapper *mapper, int bound, Qt::Orientation boundOrientation, qreal aspectRatio) {
    QSizeF sceneSize = mapper->mapFromGrid(boundOrientation == Qt::Horizontal ? QSize(bound, 0) : QSize(0, bound));
    if(boundOrientation == Qt::Horizontal) {
        sceneSize.setHeight(sceneSize.width() / aspectRatio);
    } else {
        sceneSize.setWidth(sceneSize.height() * aspectRatio);
    }

    QSize gridSize0 = mapper->mapToGrid(sceneSize);
    QSize gridSize1 = gridSize0;
    if(boundOrientation == Qt::Horizontal) {
        gridSize1.setHeight(qMax(gridSize1.height() - 1, 1));
    } else {
        gridSize1.setWidth(qMax(gridSize1.width() - 1, 1));
    }

    qreal distance0 = std::abs(std::log(SceneUtility::aspectRatio(mapper->mapFromGrid(gridSize0)) / aspectRatio)) / 1.25; /* Prefer larger size. */
    qreal distance1 = std::abs(std::log(SceneUtility::aspectRatio(mapper->mapFromGrid(gridSize1)) / aspectRatio));
    return distance0 < distance1 ? gridSize0 : gridSize1;
}

inline QSize bestDoubleBoundedSize(QnWorkbenchGridMapper *mapper, const QSize &bound, qreal aspectRatio) {
    qreal boundAspectRatio = SceneUtility::aspectRatio(mapper->mapFromGrid(bound));

    if(aspectRatio < boundAspectRatio) {
        return bestSingleBoundedSize(mapper, bound.height(), Qt::Vertical, aspectRatio);
    } else {
        return bestSingleBoundedSize(mapper, bound.width(), Qt::Horizontal, aspectRatio);
    }
}

inline QRect bestDoubleBoundedGeometry(QnWorkbenchGridMapper *mapper, const QRect &bound, qreal aspectRatio) {
    QSize size = bestDoubleBoundedSize(mapper, bound.size(), aspectRatio);

    return QRect(
        bound.topLeft() + SceneUtility::toPoint(bound.size() - size) / 2,
        size
    );
}

#endif // QN_WORKBENCH_UTILITY_H
