// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "jump_to_live_button.h"
#include "private/jump_to_live_button_p.h"

namespace nx::vms::client::desktop {

JumpToLiveButton::JumpToLiveButton(QGraphicsItem* parent,
    const std::optional<QColor>& pressedColor,
    const std::optional<QColor>& activeColor,
    const std::optional<QColor>& normalColor)
    :
    base_type(parent, pressedColor, activeColor, normalColor),
    d_ptr(new JumpToLiveButtonPrivate(this))
{
}

JumpToLiveButton::~JumpToLiveButton()
{
    if (!isPressed() || !isEnabled())
        return;

    setPressed(false);
    emit released();
}

QString JumpToLiveButton::toolTip() const
{
    Q_D(const JumpToLiveButton);
    return d->toolTip();
}

void JumpToLiveButton::setToolTip(const QString& toolTip)
{
    Q_D(JumpToLiveButton);
    d->setToolTip(toolTip);
}

Qt::Edge JumpToLiveButton::toolTipEdge() const
{
    Q_D(const JumpToLiveButton);
    return d->toolTipEdge();
}

void JumpToLiveButton::setToolTipEdge(Qt::Edge edge)
{
    Q_D(JumpToLiveButton);
    d->setToolTipEdge(edge);
}

void JumpToLiveButton::paint(QPainter* painter,
    const QStyleOptionGraphicsItem* option, QWidget* widget)
{
    Q_D(JumpToLiveButton);
    d->paint(painter, option, widget);
}

} // namespace nx::vms::client::desktop
