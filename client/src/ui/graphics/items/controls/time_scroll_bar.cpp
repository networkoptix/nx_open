#include "time_scroll_bar.h"

#include <QtWidgets/QStyleOptionSlider>

#include <utils/common/scoped_painter_rollback.h>

#include <ui/style/noptix_style.h>

QnTimeScrollBar::QnTimeScrollBar(QGraphicsItem *parent):
    base_type(Qt::Horizontal, parent),
    m_indicatorPosition(0)
{}

QnTimeScrollBar::~QnTimeScrollBar() {
    return;
}

const QnTimeScrollBarColors &QnTimeScrollBar::colors() const {
    return m_colors;
}

void QnTimeScrollBar::setColors(const QnTimeScrollBarColors &colors) {
    m_colors = colors;
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

    QnScopedPainterPenRollback penRollback(painter);
    QnScopedPainterBrushRollback brushRollback(painter);
    QnScopedPainterAntialiasingRollback antialiasingRollback(painter, false);

    /* Draw frame. */
    painter->setPen(QPen(m_colors.border, 0));
    painter->setBrush(Qt::NoBrush);
    painter->drawRect(rect());

    /* Draw scrollbar handle. */
    QRect handleRect = style()->subControlRect(QStyle::CC_ScrollBar, &opt, QStyle::SC_ScrollBarSlider, this);
    painter->setPen(QPen(m_colors.border, 0));
    painter->setBrush(m_colors.handle);
    painter->drawRect(handleRect);

    antialiasingRollback.rollback();

    /* Draw indicator. */
    if(m_indicatorPosition >= minimum() && indicatorPosition() <= maximum() + pageStep()) {
        /* Calculate handle- and groove-relative indicator positions. */
        qint64 handleValue = qBound(0ll, m_indicatorPosition - sliderPosition(), pageStep());
        qint64 grooveValue = m_indicatorPosition - handleValue;

        /* Calculate handle- and groove-relative indicator offsets. */
        qreal grooveOffset = positionFromValue(grooveValue).x();
        qreal handleOffset = GraphicsStyle::sliderPositionFromValue(0, pageStep(), handleValue, handleRect.width(), opt.upsideDown, true);

        /* Paint it. */
        qreal x = handleOffset + grooveOffset;
        painter->setPen(QPen(m_colors.indicator, 0));
        painter->drawLine(QPointF(x, opt.rect.top() + 1.0), QPointF(x, opt.rect.bottom())); /* + 1.0 is to deal with AA spilling the line outside the item's boundaries. */
    }
}

void QnTimeScrollBar::contextMenuEvent(QGraphicsSceneContextMenuEvent *) {
    return; /* No default context menu. */
}
