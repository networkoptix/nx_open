#include "grid_highlight_item.h"
#include <QtGui/QPainter>

QnGridHighlightItem::QnGridHighlightItem(QGraphicsItem *parent):
    QGraphicsObject(parent),
    m_color(QColor(0, 0, 0, 0))
{
    setAcceptedMouseButtons(0);
    
    /* Don't disable this item here. When disabled, it starts accepting wheel events 
     * (and probably other events too). Looks like a Qt bug. */
}

QRectF QnGridHighlightItem::boundingRect() const {
    return m_rect;
}

QnGridHighlightItem::~QnGridHighlightItem() {
    return;
}

void QnGridHighlightItem::setRect(const QRectF &rect) {
    prepareGeometryChange();

    m_rect = rect;
}

void QnGridHighlightItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *) {
    painter->fillRect(m_rect, m_color);
}

