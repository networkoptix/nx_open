#include "simple_frame_widget.h"

#include <utils/common/scoped_painter_rollback.h>

#include <ui/common/palette.h>

QnSimpleFrameWidget::QnSimpleFrameWidget(QGraphicsItem *parent, Qt::WindowFlags windowFlags):
    base_type(parent, windowFlags),
    m_frameWidth(1.0),
    m_frameStyle(Qt::SolidLine)
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

Qt::PenStyle QnSimpleFrameWidget::frameStyle() const {
    return m_frameStyle;
}

void QnSimpleFrameWidget::setFrameStyle(Qt::PenStyle frameStyle) {
    if(m_frameStyle == frameStyle)
        return;

    m_frameStyle = frameStyle;
    update();
}

QBrush QnSimpleFrameWidget::frameBrush() const {
    return palette().color(QPalette::Highlight);
}

void QnSimpleFrameWidget::setFrameBrush(const QBrush &frameBrush) {
    /* Note that we cannot optimize the setPalette call away because palette stores
     * bitmask of changed colors for palette inheritance handling. */
    setPaletteBrush(this, QPalette::Highlight, frameBrush);
}

QColor QnSimpleFrameWidget::frameColor() const {
    return frameBrush().color();
}

void QnSimpleFrameWidget::setFrameColor(const QColor &frameColor) {
    setFrameBrush(frameColor);
}

QBrush QnSimpleFrameWidget::windowBrush() const {
    return palette().brush(QPalette::Window);
}

void QnSimpleFrameWidget::setWindowBrush(const QBrush &windowBrush) {
    /* Note that we cannot optimize the setPalette call away because palette stores
     * bitmask of changed colors for palette inheritance handling. */
    setPaletteBrush(this, QPalette::Window, windowBrush);
}

QColor QnSimpleFrameWidget::windowColor() const {
    return windowBrush().color();
}

void QnSimpleFrameWidget::setWindowColor(const QColor &windowColor) {
    setWindowBrush(windowColor);
}

void QnSimpleFrameWidget::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *) {
    QnScopedPainterPenRollback penRollback(painter, QPen(frameBrush(), m_frameWidth, m_frameStyle));
    QnScopedPainterBrushRollback brushRollback(painter, windowBrush());
    painter->drawRect(rect().adjusted(m_frameWidth, m_frameWidth, -m_frameWidth, -m_frameWidth));
}
