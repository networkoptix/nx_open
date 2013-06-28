#include "particle_item.h"

#include <QRadialGradient>

#include <utils/common/scoped_painter_rollback.h>
#include <utils/math/color_transformations.h>
#include <utils/math/fuzzy.h>

QnParticleItem::QnParticleItem(QGraphicsItem *parent):
    base_type(parent) 
{
    setColor(Qt::black);
}

QnParticleItem::~QnParticleItem() {
    return;
}

void QnParticleItem::setColor(const QColor &color) {
    if(m_color == color)
        return;

    m_color = color;

    QRadialGradient gradient(0.5, 0.5, 0.5);
    gradient.setCoordinateMode(QGradient::ObjectBoundingMode);
    gradient.setColorAt(0.0, color);
    gradient.setColorAt(1.0, toTransparent(color));
    m_brush = QBrush(gradient);

    update();
}

void QnParticleItem::setRect(const QRectF &rect) {
    if(qFuzzyCompare(m_rect, rect))
        return;

    prepareGeometryChange();

    m_rect = rect;
}

void QnParticleItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) {
    QnScopedPainterBrushRollback brushRollback(painter, m_brush);
    QnScopedPainterPenRollback penRollback(painter, Qt::NoPen);
    painter->drawRect(rect());
}
