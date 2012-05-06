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
    base_type(Qt::Horizontal, parent),
    m_indicatorPosition(0)
{}

QnTimeScrollBar::~QnTimeScrollBar() {
    return;
}

qint64 QnTimeScrollBar::indicatorPosition() const {
    return m_indicatorPosition;
}

void QnTimeScrollBar::setIndicatorPosition(qint64 indicatorPosition) {
    m_indicatorPosition = indicatorPosition;
}

void QnTimeScrollBar::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *widget) {
    sendPendingMouseMoves(widget);
    
    QStyleOptionSlider opt;
    initStyleOption(&opt);

    QnScopedPainterAntialiasingRollback antialiasingRollback(painter, false);

    /* Draw scrollbar handle. */
    QRect handleRect = style()->subControlRect(QStyle::CC_ScrollBar, &opt, QStyle::SC_ScrollBarSlider, this);
    {
        QnScopedPainterPenRollback penRollback(painter, QPen(separatorColor, 0));
        QnScopedPainterBrushRollback brushRollback(painter, handleColor);
        painter->drawRect(handleRect);
    }

    /* Draw indicator. */
    if(m_indicatorPosition >= minimum() && indicatorPosition() <= maximum() + pageStep()) {
        /* Calculate handle- and groove-relative indicator positions. */
        qint64 handleValue = qBound(0ll, m_indicatorPosition - sliderPosition(), pageStep());
        qint64 grooveValue = m_indicatorPosition - handleValue;

        /* Calculate handle- and groove-relative indicator offsets. */
        qreal grooveOffset = positionFromValue(grooveValue).x();
        qreal handleOffset = GraphicsStyle::sliderPositionFromValue(0, pageStep(), handleValue, handleRect.width(), opt.upsideDown);

        /* Paint it. */
        qreal x = handleOffset + grooveOffset;
        QnScopedPainterPenRollback penRollback(painter, QPen(indicatorColor, 0));
        painter->drawLine(QPointF(x, opt.rect.top()), QPointF(x, opt.rect.bottom()));
    }
}


