#include "software_trigger_button.h"
#include "private/software_trigger_button_p.h"

namespace nx {
namespace client {
namespace desktop {
namespace ui {
namespace graphics {

SoftwareTriggerButton::SoftwareTriggerButton(QGraphicsItem* parent):
    base_type(parent),
    d_ptr(new SoftwareTriggerButtonPrivate(this))
{
}

SoftwareTriggerButton::~SoftwareTriggerButton()
{
    if (!isPressed() || !isEnabled())
        return;

    setPressed(false);
    emit released();
}

QString SoftwareTriggerButton::toolTip() const
{
    Q_D(const SoftwareTriggerButton);
    return d->toolTip();
}

void SoftwareTriggerButton::setToolTip(const QString& toolTip)
{
    Q_D(SoftwareTriggerButton);
    d->setToolTip(toolTip);
}

Qt::Edge SoftwareTriggerButton::toolTipEdge() const
{
    Q_D(const SoftwareTriggerButton);
    return d->toolTipEdge();
}

void SoftwareTriggerButton::setToolTipEdge(Qt::Edge edge)
{
    Q_D(SoftwareTriggerButton);
    d->setToolTipEdge(edge);
}

QSize SoftwareTriggerButton::buttonSize() const
{
    Q_D(const SoftwareTriggerButton);
    return d->buttonSize();
}

void SoftwareTriggerButton::setButtonSize(const QSize& size)
{
    Q_D(SoftwareTriggerButton);
    d->setButtonSize(size);
}

void SoftwareTriggerButton::setIcon(const QString& name)
{
    Q_D(SoftwareTriggerButton);
    d->setIcon(name);
}

bool SoftwareTriggerButton::prolonged() const
{
    Q_D(const SoftwareTriggerButton);
    return d->prolonged();
}

void SoftwareTriggerButton::setProlonged(bool value)
{
    Q_D(SoftwareTriggerButton);
    d->setProlonged(value);
}

SoftwareTriggerButton::State SoftwareTriggerButton::state() const
{
    Q_D(const SoftwareTriggerButton);
    return d->state();
}

void SoftwareTriggerButton::setState(State state)
{
    Q_D(SoftwareTriggerButton);
    d->setState(state);
}

bool SoftwareTriggerButton::isLive() const
{
    Q_D(const SoftwareTriggerButton);
    return d->isLive();
}

void SoftwareTriggerButton::setLive(bool value)
{
    Q_D(SoftwareTriggerButton);
    if (d->setLive(value))
        emit isLiveChanged();
}

void SoftwareTriggerButton::paint(QPainter* painter,
    const QStyleOptionGraphicsItem* option, QWidget* widget)
{
    Q_D(SoftwareTriggerButton);
    d->paint(painter, option, widget);
}

} // namespace graphics
} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
