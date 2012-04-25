#include "time_scroll_bar.h"

#include <QtGui/QStyleOptionSlider>

#include <utils/common/scoped_painter_rollback.h>

#include <ui/style/noptix_style.h>

namespace {
    const QColor indicatorColor(255, 255, 255, 255);

    const QColor separatorColor(255, 255, 255, 64);
    const QColor handleColor(255, 255, 255, 48);

} // anonymous namespace

QnTimeScrollBar::QnTimeScrollBar(QGraphicsItem *parent):
    base_type(Qt::Horizontal, parent) 
{
    
}

QnTimeScrollBar::~QnTimeScrollBar() {
    return;
}

qint64 QnTimeScrollBar::indicatorPosition() const {
    return m_indicatorPosition;
}

void QnTimeScrollBar::setIndicatorPosition(qint64 indicatorPosition) {
    m_indicatorPosition = indicatorPosition;
}

void QnTimeScrollBar::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) {
    QStyleOptionSlider opt;
    initStyleOption(&opt);

    QnScopedPainterAntialiasingRollback antialiasingRollback(painter, false);

    /* Draw scrollbar handle. */
    {
        QRect handleRect = style()->subControlRect(QStyle::CC_ScrollBar, &opt, QStyle::SC_ScrollBarSlider, this);

        QnScopedPainterPenRollback penRollback(painter, QPen(separatorColor, 0));
        QnScopedPainterBrushRollback brushRollback(painter, handleColor);
        painter->drawRect(handleRect);
    }

    /* Draw indicator. */
    if(m_indicatorPosition >= minimum() && indicatorPosition() <= maximum() + pageStep()) {
        QRect grooveRect = style()->subControlRect(QStyle::CC_Slider, &opt, QStyle::SC_SliderGroove, this);
        qreal x = grooveRect.left() + GraphicsStyle::sliderPositionFromValue(minimum(), maximum() + pageStep(), m_indicatorPosition, grooveRect.width(), opt.upsideDown);

        QnScopedPainterPenRollback penRollback(painter, QPen(indicatorColor, 0));
        QRectF rect = this->rect();
        painter->drawLine(QPointF(x, rect.top()), QPointF(x, rect.bottom()));
    }
}


