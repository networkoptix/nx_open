#include "workbench_grid_mapper.h"

#include <cmath>

#include <ui/style/globals.h>

#include <nx/utils/math/fuzzy.h>
#include <nx/client/core/utils/geometry.h>

using nx::client::core::Geometry;

QnWorkbenchGridMapper::QnWorkbenchGridMapper(QObject *parent):
    QObject(parent),
    m_origin(0.0, 0.0),
    m_spacing(0.0, 0.0)
{
    qreal unit = qnGlobals->workbenchUnitSize();
    qreal cellAspectRatio = qnGlobals->defaultLayoutCellAspectRatio();
    m_cellSize = {unit, unit / cellAspectRatio};
}

QnWorkbenchGridMapper::~QnWorkbenchGridMapper() {
    return;
}

void QnWorkbenchGridMapper::setOrigin(const QPointF &origin) {
    if(qFuzzyEquals(origin, m_origin))
        return;

    m_origin = origin;

    emit originChanged();
}

void QnWorkbenchGridMapper::setCellSize(const QSizeF &cellSize) {
    if(qFuzzyEquals(cellSize, m_cellSize))
        return;

    m_cellSize = cellSize;

    emit cellSizeChanged();
}

qreal QnWorkbenchGridMapper::spacing() const
{
    return m_spacing.width();
}

QSizeF QnWorkbenchGridMapper::step() const
{
    return m_cellSize + m_spacing;
}

void QnWorkbenchGridMapper::setSpacing(qreal spacing)
{
    if(qFuzzyEquals(spacing, m_spacing.width()))
        return;

    m_spacing = QSizeF(spacing, spacing);

    emit spacingChanged();
}

QPointF QnWorkbenchGridMapper::mapToGridF(const QPointF &pos) const
{
    return Geometry::cwiseDiv(pos - m_origin, Geometry::toPoint(m_cellSize + m_spacing));
}

QPointF QnWorkbenchGridMapper::mapFromGridF(const QPointF &gridPos) const
{
    return m_origin + Geometry::cwiseMul(gridPos, Geometry::toPoint(m_cellSize + m_spacing));
}

QPoint QnWorkbenchGridMapper::mapToGrid(const QPointF &pos) const {
    QPointF gridPos = mapToGridF(pos);
    return QPoint(std::floor(gridPos.x()), std::floor(gridPos.y()));
}

QPointF QnWorkbenchGridMapper::mapFromGrid(const QPoint &gridPos) const {
    return mapFromGridF(gridPos);
}

QSizeF QnWorkbenchGridMapper::mapToGridF(const QSizeF &size) const
{
    return Geometry::cwiseDiv(size + m_spacing, m_cellSize + m_spacing);
}

QSizeF QnWorkbenchGridMapper::mapFromGridF(const QSizeF &gridSize) const
{
    return Geometry::cwiseMul(gridSize, m_cellSize + m_spacing) - m_spacing;
}

QSize QnWorkbenchGridMapper::mapToGrid(const QSizeF &size) const {
    QSizeF gridSize = mapToGridF(size);
    QSizeF ceilGridSize = QSize(qFuzzyCeil(gridSize.width()), qFuzzyCeil(gridSize.height()));

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
    QPointF topLeft = rect.center() - Geometry::toPoint(mapFromGrid(gridSize)) / 2;

    QPoint gridTopLeft = mapToGrid(topLeft + Geometry::toPoint(m_cellSize) / 2);
    return QRect(gridTopLeft, gridSize);
}

QRectF QnWorkbenchGridMapper::mapFromGrid(const QRect &gridRect) const {
    return mapFromGridF(QRectF(gridRect));
}

QPointF QnWorkbenchGridMapper::mapDeltaToGridF(const QPointF &delta) const
{
    return Geometry::cwiseDiv(delta, Geometry::toPoint(m_cellSize + m_spacing));
}
