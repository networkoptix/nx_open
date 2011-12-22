#include "workbench_grid_mapper.h"
#include <cmath>

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

QPointF QnWorkbenchGridMapper::mapToGridF(const QPointF &pos) const {
    return cwiseDiv(pos - m_origin, toPoint(m_cellSize + m_spacing));
}

QPointF QnWorkbenchGridMapper::mapFromGridF(const QPointF &gridPos) const {
    return m_origin + cwiseMul(gridPos, toPoint(m_cellSize + m_spacing));
}

QPoint QnWorkbenchGridMapper::mapToGrid(const QPointF &pos) const {
    QPointF gridPos = mapToGridF(pos);
    return QPoint(std::floor(gridPos.x()), std::floor(gridPos.y()));
}

QPointF QnWorkbenchGridMapper::mapFromGrid(const QPoint &gridPos) const {
    return mapFromGridF(gridPos);
}

QSizeF QnWorkbenchGridMapper::mapToGridF(const QSizeF &size) const {
    return cwiseDiv(size + m_spacing, m_cellSize + m_spacing);
}

QSizeF QnWorkbenchGridMapper::mapFromGridF(const QSizeF &gridSize) const {
    return cwiseMul(gridSize, m_cellSize + m_spacing) - m_spacing;
}

QSize QnWorkbenchGridMapper::mapToGrid(const QSizeF &size) const {
    QSizeF gridSize = mapToGridF(size);
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
    return mapFromGridF(gridSize);
}

QRectF QnWorkbenchGridMapper::mapToGridF(const QRectF &rect) const {
    return QRectF(
        mapToGridF(rect.topLeft()),
        mapToGridF(rect.size())
    );
}

QRectF QnWorkbenchGridMapper::mapFromGridF(const QRectF &gridRect) const {
    return QRectF(
        mapFromGridF(gridRect.topLeft()),
        mapFromGridF(gridRect.size())
    );
}

QRect QnWorkbenchGridMapper::mapToGrid(const QRectF &rect) const {
    QSize gridSize = mapToGrid(rect.size());

    /* Compute top-left corner of the rect expanded to integer cell size. */
    QPointF topLeft = rect.center() - toPoint(mapFromGrid(gridSize)) / 2;

    QPoint gridTopLeft = mapToGrid(topLeft + toPoint(m_cellSize) / 2);
    return QRect(gridTopLeft, gridSize);
}

QRectF QnWorkbenchGridMapper::mapFromGrid(const QRect &gridRect) const {
    return mapFromGridF(QRectF(gridRect));
}
