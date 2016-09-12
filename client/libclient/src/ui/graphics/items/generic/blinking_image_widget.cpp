#include "blinking_image_widget.h"

#include <ui/graphics/items/generic/particle_item.h>
#include <ui/graphics/items/generic/tool_tip_widget.h>

QnBlinkingImageButtonWidget::QnBlinkingImageButtonWidget(QGraphicsItem* parent):
    base_type(parent),
    m_time(0),
    m_count(0)
{
    registerAnimation(this);
    startListening();

    m_particle = new QnParticleItem(this);

    m_balloon = new QnToolTipWidget(this);
    m_balloon->setText(tr("You have new notifications."));
    m_balloon->setOpacity(0.0);

    connect(m_balloon,  &QGraphicsWidget::geometryChanged,  this, &QnBlinkingImageButtonWidget::updateBalloonTailPos);
    connect(this,       &QGraphicsWidget::geometryChanged,  this, &QnBlinkingImageButtonWidget::updateParticleGeometry);
    connect(this,       &QnImageButtonWidget::toggled,      this, &QnBlinkingImageButtonWidget::updateParticleVisibility);
    connect(m_particle, &QGraphicsObject::visibleChanged,   this, &QnBlinkingImageButtonWidget::at_particle_visibleChanged);

    updateParticleGeometry();
    updateParticleVisibility();
    updateToolTip();
    updateBalloonTailPos();
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

void QnBlinkingImageButtonWidget::showBalloon()
{
    /*updateBalloonGeometry();
    opacityAnimator(m_balloon, 4.0)->animateTo(1.0);

    QTimer::singleShot(3000, this, SLOT(hideBalloon()));*/
}

void QnBlinkingImageButtonWidget::hideBalloon()
{
    /*opacityAnimator(m_balloon, 4.0)->animateTo(0.0);*/
}

void QnBlinkingImageButtonWidget::updateParticleGeometry()
{
    QRectF rect = this->rect();
    qreal radius = (rect.width() + rect.height()) / 4.0;

    m_particle->setRect(QRectF(rect.center() - QPointF(radius, radius), 2.0 * QSizeF(radius, radius)));
}

void QnBlinkingImageButtonWidget::updateParticleVisibility()
{
    m_particle->setVisible(m_count > 0 && !isChecked());
}

void QnBlinkingImageButtonWidget::updateToolTip()
{
    setToolTip(tr("You have %n notifications", "", m_count));
}

void QnBlinkingImageButtonWidget::updateBalloonTailPos()
{
    QRectF rect = m_balloon->rect();
    m_balloon->setTailPos(QPointF(rect.right() + 8.0, rect.center().y()));
}

void QnBlinkingImageButtonWidget::updateBalloonGeometry()
{
    QRectF rect = this->rect();
    m_balloon->pointTo(QPointF(rect.left(), rect.center().y()));
}

void QnBlinkingImageButtonWidget::tick(int deltaMSecs)
{
    m_time += deltaMSecs;
    m_particle->setOpacity(0.6 + 0.4 * std::sin(m_time / 1000.0 * 2 * M_PI));
}

void QnBlinkingImageButtonWidget::at_particle_visibleChanged()
{
    if (m_particle->isVisible())
        showBalloon();
}

