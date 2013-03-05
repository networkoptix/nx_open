#include "bordered_popup_widget.h"

#include <QPaintEvent>
#include <QPen>
#include <QPainter>
#include <QPainterPath>

#include <ui/style/globals.h>

QnBorderedPopupWidget::QnBorderedPopupWidget(QWidget *parent) :
    base_type(parent),
    m_borderColor(qnGlobals->selectedFrameColor())
{
}

QnBorderedPopupWidget::~QnBorderedPopupWidget() {
}

void QnBorderedPopupWidget::paintEvent(QPaintEvent *event) {
    base_type::paintEvent(event);

    QPainter p(this);

    QPainterPath path;
    path.addRoundedRect(this->rect().adjusted(3, 3, -6, -6), 3, 3);

    QPen pen;
    pen.setColor(m_borderColor);
    pen.setWidthF(3);
    p.strokePath(path, pen);
}

void QnBorderedPopupWidget::setBorderColor(QColor color) {
    m_borderColor = color;
}

QColor QnBorderedPopupWidget::borderColor() const {
    return m_borderColor;
}
