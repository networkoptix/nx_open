#include "simple_frame_widget.h"
#include <utils/common/scoped_painter_rollback.h>

QnSimpleFrameWidget::QnSimpleFrameWidget(QGraphicsItem *parent, Qt::WindowFlags windowFlags):
    base_type(parent, windowFlags),
    m_frameWidth(1.0)
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

QBrush QnSimpleFrameWidget::frameBrush() const {
    return palette().color(QPalette::Highlight);
}

void QnSimpleFrameWidget::setFrameBrush(const QBrush &frameBrush) {
    /* Note that we cannot optimize the setPalette call away because palette stores
     * bitmask of changed colors for palette inheritance handling. */

    QPalette palette = this->palette();
    palette.setBrush(QPalette::Highlight, frameBrush);
    setPalette(palette);
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

    QPalette palette = this->palette();
    palette.setBrush(QPalette::Window, windowBrush);
    setPalette(palette);
}

QColor QnSimpleFrameWidget::windowColor() const {
    return windowBrush().color();
}

void QnSimpleFrameWidget::setWindowColor(const QColor &windowColor) {
    setWindowBrush(windowColor);
}

void QnSimpleFrameWidget::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) {
    QnScopedPainterPenRollback penRollback(painter, QPen(frameBrush(), m_frameWidth));
    QnScopedPainterBrushRollback brushRollback(painter, windowBrush());
    painter->drawRect(rect().adjusted(m_frameWidth, m_frameWidth, -m_frameWidth, -m_frameWidth));
}
