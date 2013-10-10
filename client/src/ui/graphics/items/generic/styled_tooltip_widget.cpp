
#include "styled_tooltip_widget.h"

#include <limits>

#include <QtWidgets/QApplication>

#include <ui/common/palette.h>


QnStyledTooltipWidget::QnStyledTooltipWidget(QGraphicsItem *parent):
    base_type(parent)
{
    setContentsMargins(5.0, 5.0, 5.0, 5.0);
    setTailWidth(5.0);

    setPaletteColor(this, QPalette::WindowText, QColor(63, 159, 216));
    setWindowBrush(QColor(0, 0, 0, 255));
    setFrameBrush(QColor(203, 210, 233, 128));
    setFrameWidth(1.0);

    QFont fixedFont = QApplication::font();
    fixedFont.setPixelSize(14);
    setFont(fixedFont);

    setZValue(std::numeric_limits<qreal>::max());
    updateTailPos();
}

QnStyledTooltipWidget::~QnStyledTooltipWidget() {}

void QnStyledTooltipWidget::setGeometry(const QRectF &rect) {
    QSizeF oldSize = size();

    base_type::setGeometry(rect);

    if(size() != oldSize)
        updateTailPos();
}

void QnStyledTooltipWidget::updateTailPos() {
    QRectF rect = this->rect();
    setTailPos(QPointF((rect.left() + rect.right()) / 2, rect.bottom() + 10.0));
}
