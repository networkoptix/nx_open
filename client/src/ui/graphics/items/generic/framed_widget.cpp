#include "framed_widget.h"

#include <utils/common/scoped_painter_rollback.h>

#include <ui/common/palette.h>

// -------------------------------------------------------------------------- //
// FramedBase
// -------------------------------------------------------------------------- //
FramedBase::FramedBase():
    m_self(NULL),
    m_frameWidth(1.0),
    m_roundingRadius(0.0),
    m_frameStyle(Qt::SolidLine),
    m_frameShape(Qn::RectangularFrame)
{}

FramedBase::~FramedBase() {
    return;
}

void FramedBase::initSelf(QGraphicsWidget *self) {
    m_self = self;
}

qreal FramedBase::frameWidth() const {
    return m_frameWidth;
}

void FramedBase::setFrameWidth(qreal frameWidth) {
    if(qFuzzyCompare(m_frameWidth, frameWidth))
        return;

    m_frameWidth = frameWidth;
    m_self->update();
}

Qn::FrameShape FramedBase::frameShape() const {
    return m_frameShape;
}

void FramedBase::setFrameShape(Qn::FrameShape frameShape) {
    if(m_frameShape == frameShape)
        return;

    m_frameShape = frameShape;
    m_self->update();
}

Qt::PenStyle FramedBase::frameStyle() const {
    return m_frameStyle;
}

void FramedBase::setFrameStyle(Qt::PenStyle frameStyle) {
    if(m_frameStyle == frameStyle)
        return;

    m_frameStyle = frameStyle;
    m_self->update();
}

QBrush FramedBase::frameBrush() const {
    return m_self->palette().color(QPalette::Highlight);
}

void FramedBase::setFrameBrush(const QBrush &frameBrush) {
    /* Note that we cannot optimize the setPalette call away because palette stores
     * bitmask of changed colors for palette inheritance handling. */
    setPaletteBrush(m_self, QPalette::Highlight, frameBrush);
}

QColor FramedBase::frameColor() const {
    return frameBrush().color();
}

void FramedBase::setFrameColor(const QColor &frameColor) {
    setFrameBrush(frameColor);
}

QBrush FramedBase::windowBrush() const {
    return m_self->palette().brush(QPalette::Window);
}

void FramedBase::setWindowBrush(const QBrush &windowBrush) {
    /* Note that we cannot optimize the setPalette call away because palette stores
     * bitmask of changed colors for palette inheritance handling. */
    setPaletteBrush(m_self, QPalette::Window, windowBrush);
}

QColor FramedBase::windowColor() const {
    return windowBrush().color();
}

void FramedBase::setWindowColor(const QColor &windowColor) {
    setWindowBrush(windowColor);
}

qreal FramedBase::roundingRadius() const {
    return m_roundingRadius;
}
void FramedBase::setRoundingRadius(qreal roundingRadius) {
    if(qFuzzyCompare(m_roundingRadius, roundingRadius))
        return;

    m_roundingRadius = roundingRadius;
    m_self->update();
}

void FramedBase::paintFrame(QPainter *painter, const QRectF &rect) {
    if(m_frameShape == Qn::NoFrame)
        return;

    QnScopedPainterPenRollback penRollback(painter, QPen(frameBrush(), m_frameWidth, m_frameStyle, Qt::SquareCap, Qt::MiterJoin));
    QnScopedPainterBrushRollback brushRollback(painter, windowBrush());

    qreal d = m_frameWidth / 2.0;
    QRectF frameRect = rect.adjusted(d, d, -d, -d);

    switch (m_frameShape) {
    case Qn::RectangularFrame: {
        QBrush frameBrush = this->frameBrush();
        QBrush windowBrush = this->windowBrush();
        if(frameBrush.style() == Qt::SolidPattern && windowBrush.style() == Qt::SolidPattern) {
            /* For some reason this code works WAY faster. */
            qreal l = rect.left(), t = rect.top(), w = rect.width(), h = rect.height();
            qreal fw = m_frameWidth;

            painter->setPen(Qt::NoPen);
            painter->fillRect(QRectF(l + fw,        t + fw,     w - 2 * fw, h - 2 * fw), windowBrush);
            painter->fillRect(QRectF(l,             t,          w,          fw),         frameBrush);
            painter->fillRect(QRectF(l,             t + h - fw, w,          fw),         frameBrush);
            painter->fillRect(QRectF(l,             t + fw,     fw,         h - 2 * fw), frameBrush);
            painter->fillRect(QRectF(l + w - fw,    t + fw,     fw,         h - 2 * fw), frameBrush);
        } else {
            painter->drawRect(rect);
        }
        break;
    }
    case Qn::RoundedRectangularFrame:
        painter->drawRoundedRect(rect, m_roundingRadius, m_roundingRadius, Qt::AbsoluteSize);
        break;
    case Qn::EllipticalFrame:
        painter->drawEllipse(frameRect);
        break;
    default:
        break;
    }
}
