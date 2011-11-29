#include "image_button_widget.h"
#include <QPainter>

QnImageButtonWidget::QnImageButtonWidget(QGraphicsItem *parent):
    base_type(parent)
{
    setAcceptedMouseButtons(Qt::LeftButton);
    setClickableButtons(Qt::LeftButton);
}

void QnImageButtonWidget::setPixmap(const QPixmap &pixmap) {
    m_pixmap = pixmap;
}

void QnImageButtonWidget::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) {
    if(m_pixmap.isNull()) {
        base_type::paint(painter, option, widget);
        return;
    }

    painter->drawPixmap(rect(), m_pixmap, m_pixmap.rect());
}

void QnImageButtonWidget::clickedNotify(QGraphicsSceneMouseEvent *) {
    emit clicked();
}
