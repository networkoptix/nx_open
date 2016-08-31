
#include "frame_painter.h"

QnFramePainter::QnFramePainter()
    :
    m_dirty(true),
    m_rect(),
    m_frameWidth(1),
    m_color()
{
}

QnFramePainter::~QnFramePainter()
{
}

void QnFramePainter::setColor(const QColor& color)
{
    m_color = color;
}

QColor QnFramePainter::color() const
{
    return m_color;
}


void QnFramePainter::setBoundingRect(const QRectF& rect)
{
    if (m_rect == rect)
        return;

    m_rect = rect;
    m_dirty = true;
}

void QnFramePainter::setFrameWidth(qreal frameWidth)
{
    if (m_frameWidth == frameWidth)
        return;

    m_frameWidth = frameWidth;
    m_dirty = true;
}

qreal QnFramePainter::frameWidth() const
{
    return m_frameWidth;
}

void QnFramePainter::updateBoundingRects()
{
    if (!m_dirty)
        return;

    const auto bottomLeft = (m_rect.bottomLeft() - QPointF(0, m_frameWidth));
    const auto innerTopLeft = (m_rect.topLeft() + QPointF(0, m_frameWidth));
    const auto innerTopRight = (m_rect.topRight() + QPointF(-m_frameWidth, m_frameWidth));
    const auto verticalBorderSize = QSizeF(m_frameWidth, m_rect.height() - 2 * m_frameWidth);

    m_bounds =
    {
        QRectF(m_rect.topLeft(), QSizeF(m_rect.width(), m_frameWidth)), //< Top border
        QRectF(bottomLeft, QSizeF(m_rect.width(), m_frameWidth)),       //< Bottom border
        QRectF(innerTopLeft, verticalBorderSize),                       //< Left border
        QRectF(innerTopRight, verticalBorderSize)                       //< Right border
    };

    m_dirty = false;
}


void QnFramePainter::paint(QPainter& painter)
{
    updateBoundingRects();

    for (const auto rect : m_bounds)
        painter.fillRect(rect, m_color);
}
