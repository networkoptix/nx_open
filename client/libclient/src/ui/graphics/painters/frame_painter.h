
#pragma once

#include <array>
#include <QtCore/QRectF>

class QnFramePainter
{
public:
    QnFramePainter();

    ~QnFramePainter();

    void setColor(const QColor& color);
    QColor color() const;

    void setBoundingRect(const QRectF& rect);

    void setFrameWidth(qreal frameWidth);
    qreal frameWidth() const;

    void paint(QPainter& painter);

private:
    void updateBoundingRects();

private:
    bool m_dirty;
    QRectF m_rect;
    qreal m_frameWidth;
    QColor m_color;

    std::array<QRectF, 4> m_bounds;
};