
#include "graphics_path_item.h"

#include <QtGui/QPainter>


GraphicsPathItem::GraphicsPathItem(QGraphicsItem *parent):
    base_type(parent) 
{}

GraphicsPathItem::~GraphicsPathItem() {
    return;
}

QPainterPath GraphicsPathItem::path() const {
    return m_path;
}

void GraphicsPathItem::setPath(const QPainterPath &path) {
    if (m_path == path)
        return;
    invalidateBoundingRect();
    m_path = path;
    update();
}

QRectF GraphicsPathItem::calculateBoundingRect() const {
    qreal penWidth = pen().widthF();
    if (qFuzzyIsNull(penWidth))
        return m_path.controlPointRect();
    else {
        return shape().controlPointRect();
    }
}

QPainterPath GraphicsPathItem::shape() const {
    return shapeFromPath(m_path, pen());
}

void GraphicsPathItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *) {
    painter->setPen(pen());
    painter->setBrush(brush());
    painter->drawPath(m_path);

    //if (option->state & QStyle::State_Selected)
        //qt_graphicsItem_highlightSelected(this, painter, option);
}
