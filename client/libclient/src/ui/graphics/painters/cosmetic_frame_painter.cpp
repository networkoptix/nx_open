
#include "frame_painter.h"

#include <utils/common/scoped_painter_rollback.h>

namespace {

QPen makeCosmeticPen(const QColor& color, int width)
{
    QPen result(color, width);
    result.setCosmetic(true);
    return result;
}

} // namespace

QnCosmeticFramePainter::QnCosmeticFramePainter() :
    m_rect(),
    m_frameWidth(1),
    m_color()
{
}

QnCosmeticFramePainter::~QnCosmeticFramePainter()
{
}

void QnCosmeticFramePainter::setColor(const QColor& color)
{
    m_color = color;
}

QColor QnCosmeticFramePainter::color() const
{
    return m_color;
}


void QnCosmeticFramePainter::setBoundingRect(const QRectF& rect)
{
    m_rect = rect;
}

void QnCosmeticFramePainter::setFrameWidth(int frameWidth)
{
    m_frameWidth = frameWidth;
}

int QnCosmeticFramePainter::frameWidth() const
{
    return m_frameWidth;
}

void QnCosmeticFramePainter::paint(QPainter& painter)
{
    const QnScopedPainterAntialiasingRollback antialiasingRollback(&painter, false);
    const QnScopedPainterPenRollback penRollback(&painter, makeCosmeticPen(m_color, m_frameWidth));
    const QnScopedPainterBrushRollback brushRollback(&painter, QBrush(Qt::transparent));

    const auto roundedTransform = QTransform(1.0, 0.0, 0.0, 0.0, 1.0, 0.0,
        qRound(painter.transform().m31()), qRound(painter.transform().m32()), 1.0);
    const QPointF roundedPosition(qRound(m_rect.x()), qRound(m_rect.y()));
    const auto targetRect = QRectF(roundedPosition, m_rect.size());

    painter.drawRect(targetRect);
}
