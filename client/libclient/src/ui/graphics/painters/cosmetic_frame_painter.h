
#pragma once

#include <array>
#include <QtCore/QRectF>

class QnCosmeticFramePainter
{
public:
    QnCosmeticFramePainter();

    ~QnCosmeticFramePainter();

    void setColor(const QColor& color);
    QColor color() const;

    void setBoundingRect(const QRectF& rect);

    void setFrameWidth(int frameWidth);
    int frameWidth() const;

    void paint(QPainter& painter);

private:
    QRectF m_rect;
    int m_frameWidth;
    QColor m_color;

    std::array<QRectF, 4> m_bounds;
};