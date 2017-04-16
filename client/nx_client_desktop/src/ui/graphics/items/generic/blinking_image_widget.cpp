#include "blinking_image_widget.h"

#include <cmath>

#include <ui/graphics/items/generic/particle_item.h>
#include <ui/graphics/items/generic/tool_tip_widget.h>

QnBlinkingImageButtonWidget::QnBlinkingImageButtonWidget(QGraphicsItem* parent):
    base_type(parent),
    m_particle(new QnParticleItem(this)),
    m_time(0),
    m_count(0)
{
    registerAnimation(this);
    startListening();

    connect(this, &QGraphicsWidget::geometryChanged, this,
        &QnBlinkingImageButtonWidget::updateParticleGeometry);
    connect(this, &QnImageButtonWidget::toggled, this,
        &QnBlinkingImageButtonWidget::updateParticleVisibility);

    updateParticleGeometry();
    updateParticleVisibility();
    updateToolTip();
}

void QnBlinkingImageButtonWidget::setNotificationCount(int count)
{
    if (m_count == count)
        return;

    m_count = count;

    updateParticleVisibility();
    updateToolTip();
}

void QnBlinkingImageButtonWidget::setColor(const QColor& color)
{
    m_particle->setColor(color);
}

void QnBlinkingImageButtonWidget::updateParticleGeometry()
{
    QRectF rect = this->rect();
    qreal radius = (rect.width() + rect.height()) / 4.0;

    m_particle->setRect(QRectF(rect.center() - QPointF(radius, radius),
        2.0 * QSizeF(radius, radius)));
}

void QnBlinkingImageButtonWidget::updateParticleVisibility()
{
    m_particle->setVisible(m_count > 0 && !isChecked());
}

void QnBlinkingImageButtonWidget::updateToolTip()
{
    setToolTip(tr("You have %n notifications", "", m_count));
}

void QnBlinkingImageButtonWidget::tick(int deltaMSecs)
{
    m_time += deltaMSecs;
    m_particle->setOpacity(0.6 + 0.4 * std::sin(m_time / 1000.0 * 2 * M_PI));
}
