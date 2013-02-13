#include "abstract_graphics_shape_item.h"

AbstractGraphicsShapeItem::AbstractGraphicsShapeItem(QGraphicsItem *parent):
    base_type(parent),
    m_boundingRectValid(false)
{}

AbstractGraphicsShapeItem::~AbstractGraphicsShapeItem() {
    return;
}

QPen AbstractGraphicsShapeItem::pen() const {
    return m_pen;
}

void AbstractGraphicsShapeItem::setPen(const QPen &pen) {
    invalidateBoundingRect();
    m_pen = pen;
    update();
}

QBrush AbstractGraphicsShapeItem::brush() const {
    return m_brush;
}

void AbstractGraphicsShapeItem::setBrush(const QBrush &brush) {
    m_brush = brush;
    update();
}

void AbstractGraphicsShapeItem::invalidateBoundingRect() {
    prepareGeometryChange();

    m_boundingRectValid = false;
}

void AbstractGraphicsShapeItem::ensureBoundingRect() const {
    if(m_boundingRectValid)
        return;

    m_boundingRect = calculateBoundingRect();
    m_boundingRectValid = true;
}

QRectF AbstractGraphicsShapeItem::boundingRect() const {
    ensureBoundingRect();

    return m_boundingRect;
}

QPainterPath AbstractGraphicsShapeItem::opaqueArea() const {
    if (m_brush.isOpaque())
        return isClipped() ? clipPath() : shape();
    return base_type::opaqueArea();
}

QPainterPath AbstractGraphicsShapeItem::shapeFromPath(const QPainterPath &path, const QPen &pen) {
    /* Note: the code is from qgraphicsitem.cpp, function qt_graphicsItem_shapeFromPath. */

    /* We unfortunately need this hack as QPainterPathStroker will set a width of 1.0
     * if we pass a value of 0.0 to QPainterPathStroker::setWidth(). */
    const qreal penWidthZero = qreal(0.00000001);

    if (path == QPainterPath())
        return path;
    QPainterPathStroker ps;
    ps.setCapStyle(pen.capStyle());
    if (pen.widthF() <= 0.0)
        ps.setWidth(penWidthZero);
    else
        ps.setWidth(pen.widthF());
    ps.setJoinStyle(pen.joinStyle());
    ps.setMiterLimit(pen.miterLimit());
    QPainterPath p = ps.createStroke(path);
    p.addPath(path);
    return p;
}
