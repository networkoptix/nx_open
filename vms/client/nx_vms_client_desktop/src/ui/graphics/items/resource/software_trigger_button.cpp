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

void SoftwareTriggerButton::paint(QPainter* painter,
    const QStyleOptionGraphicsItem* option, QWidget* widget)
{
    Q_D(SoftwareTriggerButton);
    d->paint(painter, option, widget);
}

} // namespace nx::vms::client::desktop
