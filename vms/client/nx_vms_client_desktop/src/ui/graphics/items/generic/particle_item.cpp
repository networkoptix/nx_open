// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "particle_item.h"

#include <QtGui/QRadialGradient>

#include <utils/common/scoped_painter_rollback.h>
#include <utils/math/color_transformations.h>
#include <nx/utils/math/fuzzy.h>
#include <nx/vms/client/core/skin/color_theme.h>

QnParticleItem::QnParticleItem(QGraphicsItem *parent):
    base_type(parent)
{
    setColor(nx::vms::client::core::colorTheme()->color("particleItem"));
}

QnParticleItem::~QnParticleItem() {
    return;
}

void QnParticleItem::setColor(const QColor &color) {
    if(m_color == color)
        return;

    m_color = color;

    QRadialGradient gradient(0.5, 0.5, 0.5);
    gradient.setCoordinateMode(QGradient::ObjectMode);
    gradient.setColorAt(0.0, color);
    gradient.setColorAt(1.0, toTransparent(color));
    m_brush = QBrush(gradient);

    update();
}

void QnParticleItem::setRect(const QRectF &rect) {
    if(qFuzzyEquals(m_rect, rect))
        return;

    prepareGeometryChange();

    m_rect = rect;
}

void QnParticleItem::setPaintOpacity(qreal opacity)
{
    m_paintOpacity = opacity;
}

void QnParticleItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) {
    Q_UNUSED(option)
    Q_UNUSED(widget)

    painter->setOpacity(m_paintOpacity);
    QnScopedPainterBrushRollback brushRollback(painter, m_brush);
    QnScopedPainterPenRollback penRollback(painter, Qt::NoPen);
    painter->drawRect(rect());
}
