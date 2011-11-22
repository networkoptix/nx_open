#include "workbench_grid_mapper.h"
#include <cmath>

namespace {
    QSizeF operator*(const QSizeF &l, const QSizeF &r)
    {
        return QSizeF(l.width() * r.width(), l.height() * r.height());
    }

    QSizeF operator/(const QSizeF &l, const QSizeF &r)
    {
        return QSizeF(l.width() / r.width(), l.height() / r.height());
    }

    QPointF operator*(const QPointF &l, const QPointF &r)
    {
        return QPointF(l.x() * r.x(), l.y() * r.y());
    }

    QPointF operator/(const QPointF &l, const QPointF &r)
    {
        return QPointF(l.x() / r.x(), l.y() / r.y());
    }
}

QnWorkbenchGridMapper::QnWorkbenchGridMapper(QObject *parent):
    QObject(parent),
    m_origin(0.0, 0.0),
    m_cellSize(1.0, 1.0),
    m_spacing(0.0, 0.0)
{}

QnWorkbenchGridMapper::~QnWorkbenchGridMapper() {
    return;
}

void QnWorkbenchGridMapper::setOrigin(const QPointF &origin) {
    if(qFuzzyCompare(origin, m_origin))
        return;

    QPointF oldOrigin = m_origin;
    m_origin = origin;

    emit originChanged(oldOrigin, m_origin);
}

void QnWorkbenchGridMapper::setCellSize(const QSizeF &cellSize) {
    if(qFuzzyCompare(cellSize, m_cellSize))
        return;

    QSizeF oldCellSize = m_cellSize;
    m_cellSize = cellSize;

    emit cellSizeChanged(oldCellSize, m_cellSize);
}

void QnWorkbenchGridMapper::setSpacing(const QSizeF &spacing) {
    if(qFuzzyCompare(spacing, m_spacing))
        return;

    QSizeF oldSpacing = m_spacing;
    m_spacing = spacing;

    emit spacingChanged(oldSpacing, m_spacing);
}

void QnWorkbenchGridMapper::setVerticalSpacing(qreal spacing) {
    setSpacing(QSizeF(m_spacing.width(), spacing));
}

void QnWorkbenchGridMapper::setHorizontalSpacing(qreal spacing) {
    setSpacing(QSizeF(spacing, m_spacing.height()));
}

QPoint QnWorkbenchGridMapper::mapToGrid(const QPointF &pos) const {
    /* Compute origin and a unit vectors in the cell-based coordinate system. */
    QPointF origin = mapFromGrid(QPoint(0, 0)) - toPoint(m_spacing) / 2;
    QPointF unit = toPoint(m_cellSize + m_spacing);

    /* Perform coordinate transformation. */
    QPointF gridPos = (pos - origin) / unit;
    return QPoint(std::floor(gridPos.x()), std::floor(gridPos.y()));
}

QPointF QnWorkbenchGridMapper::mapFromGrid(const QPoint &gridPos) const {
    return m_origin + toPoint(m_cellSize + m_spacing) * gridPos;
}

QSize QnWorkbenchGridMapper::mapToGrid(const QSizeF &size) const {
    QSizeF gridSize = (size + m_spacing) / (m_cellSize + m_spacing);
    QSizeF ceilGridSize = QSize(std::ceil(gridSize.width()), std::ceil(gridSize.height()));

    /* It may have been rounded up as a result of floating-point precision issues.
     * Check and fix that. */
    if (qFuzzyCompare(ceilGridSize.width() - gridSize.width(), 1.0))
        ceilGridSize.setWidth(ceilGridSize.width() - 1);
    if (qFuzzyCompare(ceilGridSize.height() - gridSize.height(), 1.0))
        ceilGridSize.setHeight(ceilGridSize.height() - 1);

    return QSize(ceilGridSize.width(), ceilGridSize.height());
}

QSizeF QnWorkbenchGridMapper::mapFromGrid(const QSize &gridSize) const {
    return m_cellSize * gridSize + (m_spacing * (gridSize - QSize(1, 1))).expandedTo(QSizeF(0, 0));
}

QRect QnWorkbenchGridMapper::mapToGrid(const QRectF &rect) const {
    QSize gridSize = mapToGrid(rect.size());

    /* Compute top-left corner of the rect expanded to integer cell size. */
    QPointF topLeft = rect.center() - toPoint(mapFromGrid(gridSize)) / 2;

    QPoint gridTopLeft = mapToGrid(topLeft + toPoint(m_cellSize) / 2);
    return QRect(gridTopLeft, gridSize);
}

QRectF QnWorkbenchGridMapper::mapFromGrid(const QRect &gridRect) const {
    return QRectF(
        mapFromGrid(gridRect.topLeft()),
        mapFromGrid(gridRect.size())
    );
}
