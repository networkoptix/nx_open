#include "simple_frame_widget.h"
#include <utils/common/scoped_painter_rollback.h>

QnSimpleFrameWidget::QnSimpleFrameWidget(QGraphicsItem *parent):
    QGraphicsWidget(parent),
    m_frameWidth(1.0),
    m_frameColor(palette().color(QPalette::WindowText))
{}

QnSimpleFrameWidget::~QnSimpleFrameWidget() {
    return;
}

qreal QnSimpleFrameWidget::frameWidth() const {
    return m_frameWidth;
}

void QnSimpleFrameWidget::setFrameWidth(qreal frameWidth) {
    if(qFuzzyCompare(m_frameWidth, frameWidth))
        return;

    m_frameWidth = frameWidth;
    update();
}

QColor QnSimpleFrameWidget::frameColor() const {
    return m_frameColor;
}

void QnSimpleFrameWidget::setFrameColor(const QColor &frameColor) {
    if(m_frameColor == frameColor)
        return;

    m_frameColor = frameColor;
    update();
}

void QnSimpleFrameWidget::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) {
    QGraphicsWidget::paint(painter, option, widget);

    QnScopedPainterPenRollback penRollback(painter, QPen(m_frameColor, m_frameWidth));
    QnScopedPainterBrushRollback brushRollback(painter, Qt::NoBrush);
    painter->drawRect(rect().adjusted(m_frameWidth, m_frameWidth, -m_frameWidth, -m_frameWidth));
}
