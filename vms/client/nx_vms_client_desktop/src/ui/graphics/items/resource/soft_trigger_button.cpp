// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "soft_trigger_button.h"
#include "private/soft_trigger_button_p.h"

namespace nx::vms::client::desktop {

SoftTriggerButton::SoftTriggerButton(QGraphicsItem* parent):
    base_type(parent),
    d_ptr(new SoftTriggerButtonPrivate(this))
{
}

SoftTriggerButton::~SoftTriggerButton()
{
    if (!isPressed() || !isEnabled())
        return;

    setPressed(false);
    emit released();
}

void SoftTriggerButton::paint(QPainter* painter,
    const QStyleOptionGraphicsItem* option, QWidget* widget)
{
    Q_D(SoftTriggerButton);
    d->paint(painter, option, widget);
}

} // namespace nx::vms::client::desktop
