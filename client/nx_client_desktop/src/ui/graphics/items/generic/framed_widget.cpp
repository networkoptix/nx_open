#include "framed_widget.h"

#include <utils/common/scoped_painter_rollback.h>

#include <ui/common/palette.h>
#include <ui/workaround/sharp_pixmap_painting.h>

#include <nx/utils/math/fuzzy.h>

// -------------------------------------------------------------------------- //
// FramedBase
// -------------------------------------------------------------------------- //
FramedBase::FramedBase():
    m_self(nullptr),
    m_frameWidth(1.0),
    m_roundingRadius(0.0),
    m_frameStyle(Qt::SolidLine),
    m_frameShape(Qn::RectangularFrame),
    m_frameBorders(Qt::LeftEdge | Qt::RightEdge | Qt::BottomEdge | Qt::TopEdge)
{
}

FramedBase::~FramedBase()
{
}

void FramedBase::initSelf(QGraphicsWidget* self)
{
    m_self = self;
}

qreal FramedBase::frameWidth() const
{
    return m_frameWidth;
}

void FramedBase::setFrameWidth(qreal frameWidth)
{
    if (qFuzzyEquals(m_frameWidth, frameWidth))
        return;

    m_frameWidth = frameWidth;
    m_self->update();
}

Qn::FrameShape FramedBase::frameShape() const
{
    return m_frameShape;
}

void FramedBase::setFrameShape(Qn::FrameShape frameShape)
{
    if (m_frameShape == frameShape)
        return;

    m_frameShape = frameShape;
    m_self->update();
}

Qt::PenStyle FramedBase::frameStyle() const
{
    return m_frameStyle;
}

void FramedBase::setFrameStyle(Qt::PenStyle frameStyle)
{
    if (m_frameStyle == frameStyle)
        return;

    m_frameStyle = frameStyle;
    m_self->update();
}

const QVector<qreal>& FramedBase::dashPattern() const
{
    return m_dashPattern;
}

void FramedBase::setDashPattern(const QVector<qreal>& dashPattern)
{
    m_dashPattern = dashPattern;
}

QBrush FramedBase::frameBrush() const
{
    return m_self->palette().color(QPalette::Highlight);
}

void FramedBase::setFrameBrush(const QBrush& frameBrush)
{
    /* Note that we cannot optimize the setPalette call away because palette stores
     * bitmask of changed colors for palette inheritance handling. */
    setPaletteBrush(m_self, QPalette::Highlight, frameBrush);
}

QColor FramedBase::frameColor() const
{
    return frameBrush().color();
}

void FramedBase::setFrameColor(const QColor& frameColor)
{
    setFrameBrush(frameColor);
}

QBrush FramedBase::windowBrush() const
{
    return m_self->palette().brush(QPalette::Window);
}

void FramedBase::setWindowBrush(const QBrush& windowBrush)
{
    /* Note that we cannot optimize the setPalette call away because palette stores
     * bitmask of changed colors for palette inheritance handling. */
    setPaletteBrush(m_self, QPalette::Window, windowBrush);
}

QColor FramedBase::windowColor() const
{
    return windowBrush().color();
}

void FramedBase::setWindowColor(const QColor& windowColor)
{
    setWindowBrush(windowColor);
}

qreal FramedBase::roundingRadius() const
{
    return m_roundingRadius;
}
void FramedBase::setRoundingRadius(qreal roundingRadius)
{
    if (qFuzzyEquals(m_roundingRadius, roundingRadius))
        return;

    m_roundingRadius = roundingRadius;
    if (m_frameShape == Qn::RoundedRectangularFrame)
        m_self->update();
}

const QPainterPath& FramedBase::customFramePath() const
{
    return m_customFramePath;
}

void FramedBase::setCustomFramePath(const QPainterPath& customFramePath)
{
    if (m_customFramePath == customFramePath)
        return;

    m_customFramePath = customFramePath;
    m_customFramePathBoundingRect = customFramePath.boundingRect();
    if (m_frameShape == Qn::CustomFrame)
        m_self->update();
}

Qt::Edges FramedBase::frameBorders() const
{
    return m_frameBorders;
}

void FramedBase::setFrameBorders(Qt::Edges borders)
{
    if (borders != m_frameBorders)
    {
        m_frameBorders = borders;
        if (m_frameShape == Qn::RectangularFrame)
            m_self->update();
    }
}

void FramedBase::paintFrame(QPainter* painter, const QRectF& rect)
{
    if (m_frameShape == Qn::NoFrame)
        return;

    QPen pen(frameBrush(), m_frameWidth, m_frameStyle, Qt::SquareCap, Qt::MiterJoin);
    if (m_frameStyle == Qt::CustomDashLine)
        pen.setDashPattern(m_dashPattern);

    QN_SCOPED_PAINTER_PEN_ROLLBACK(painter, pen);
    QN_SCOPED_PAINTER_BRUSH_ROLLBACK(painter, windowBrush());

    qreal d = m_frameWidth / 2.0;
    QRectF frameRect = rect.adjusted(d, d, -d, -d);

    switch (m_frameShape)
    {
        case Qn::RectangularFrame:
        {
            QBrush frameBrush = this->frameBrush();
            QBrush windowBrush = this->windowBrush();

            /* For some reason this code works WAY faster than painter->drawRect(frameRect) */
            qreal l = rect.left(), t = rect.top(), w = rect.width(), h = rect.height();
            qreal fw = m_frameWidth;

            if (m_frameBorders.testFlag(Qt::TopEdge))
            {
                painter->fillRect(QRectF(l, t, w, fw), frameBrush);
                t += fw;
                h -= fw;
            }

            if (m_frameBorders.testFlag(Qt::BottomEdge))
            {
                h -= fw;
                painter->fillRect(QRectF(l, t + h, w, fw), frameBrush);
            }

            if (m_frameBorders.testFlag(Qt::LeftEdge))
            {
                painter->fillRect(QRectF(l, t, fw, h), frameBrush);
                l += fw;
                w -= fw;
            }

            if (m_frameBorders.testFlag(Qt::RightEdge))
            {
                w -= fw;
                painter->fillRect(QRectF(l + w, t, fw, h), frameBrush);
            }

            painter->fillRect(QRectF(l, t, w, h), windowBrush);
            break;
        }

        case Qn::RoundedRectangularFrame:
        {
            QnScopedPainterAntialiasingRollback rollback(painter, false);
            painter->drawRoundedRect(frameRect, m_roundingRadius, m_roundingRadius,
                Qt::AbsoluteSize);
            break;
        }

        case Qn::EllipticalFrame:
            painter->drawEllipse(frameRect);
            break;

        case Qn::CustomFrame:
        {
            QN_SCOPED_PAINTER_TRANSFORM_ROLLBACK(painter);

            painter->translate(frameRect.topLeft());

            const bool correctBoundWidth = !qFuzzyIsNull(m_customFramePathBoundingRect.width());
            const bool correctBoundHeight = !qFuzzyIsNull(m_customFramePathBoundingRect.height());

            static const qreal kDefaultScaleFactor = 1;
            qreal sx = (correctBoundWidth ? frameRect.width() / m_customFramePathBoundingRect.width() : kDefaultScaleFactor);
            qreal sy = (correctBoundHeight ? frameRect.height() / m_customFramePathBoundingRect.height() : kDefaultScaleFactor);
            painter->scale(sx, sy);

            painter->setPen(QPen(frameBrush(), m_frameWidth / ((sx + sy) / 2.0), m_frameStyle, Qt::SquareCap, Qt::MiterJoin));

            painter->drawPath(m_customFramePath);
        }

        default:
            break;
    }
}
