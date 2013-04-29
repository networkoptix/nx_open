#include "grid_raised_cone_item.h"

#include <QPainter>

QnGridRaisedConeItem::QnGridRaisedConeItem(QGraphicsItem *parent) :
    QGraphicsObject(parent),
    m_raisedWidget(NULL)
{
}

QnGridRaisedConeItem::~QnGridRaisedConeItem() {

}

QRectF QnGridRaisedConeItem::boundingRect() const {
    return m_rect;
}

const QRectF& QnGridRaisedConeItem::viewportRect() const {
    return m_rect;
}

void QnGridRaisedConeItem::setViewportRect(const QRectF &rect) {
    prepareGeometryChange();
    m_rect = rect;
}

QGraphicsWidget* QnGridRaisedConeItem::raisedWidget() const {
    return m_raisedWidget;
}

void QnGridRaisedConeItem::setRaisedWidget(QGraphicsWidget *widget, QRectF oldGeometry) {
    if (m_raisedWidget != NULL)
        disconnect(m_raisedWidget, 0, this, 0);

    m_raisedWidget = widget;
    if(m_raisedWidget == NULL)
        return;

    connect(m_raisedWidget, SIGNAL(geometryChanged()), this, SLOT(updateGeometry()));
    m_sourceRect = oldGeometry;
    updateGeometry();
}


void QnGridRaisedConeItem::updateGeometry() {
    if(m_raisedWidget == NULL)
        return;
    setViewportRect(m_raisedWidget->geometry());
}

void QnGridRaisedConeItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) {
    if(m_raisedWidget == NULL)
        return;

    painter->fillRect(m_rect, QColor(Qt::red));
    painter->fillRect(m_sourceRect, QColor(Qt::green));
}

