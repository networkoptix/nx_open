// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "software_trigger_button.h"
#include "private/software_trigger_button_p.h"

namespace nx::vms::client::desktop {

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

void SoftwareTriggerButton::paint(QPainter* painter,
    const QStyleOptionGraphicsItem* option, QWidget* widget)
{
    Q_D(SoftwareTriggerButton);
    d->paint(painter, option, widget);
}

} // namespace nx::vms::client::desktop
