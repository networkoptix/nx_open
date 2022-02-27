// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "close_button.h"

namespace nx::vms::client::desktop {

CloseButton::CloseButton(QWidget* parent) :
    HoverButton(lit("text_buttons/selectable_button_close.png"),
        lit("text_buttons/selectable_button_close_hovered.png"),
        lit("text_buttons/selectable_button_close_pressed.png"),
        parent)
{
    setFixedSize(HoverButton::sizeHint());
}

} // namespace nx::vms::client::desktop
