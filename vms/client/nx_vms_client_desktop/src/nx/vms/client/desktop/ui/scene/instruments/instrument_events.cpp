// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "instrument_events.h"

namespace nx::vms::client::desktop {
namespace ui {
namespace scene {

MouseEvent::MouseEvent(const QMouseEvent* event):
    localPosition(event->localPos()),
    windowPosition(event->windowPos()),
    screenPosition(event->screenPos()),
    button(event->button()),
    buttons(event->buttons()),
    modifiers(event->modifiers())
{
}

HoverEvent::HoverEvent(const QHoverEvent* event):
    position(event->pos())
{
}

KeyEvent::KeyEvent(const QKeyEvent* event):
    key(event->key()),
    modifiers(event->modifiers())
{
}

} // namespace scene
} // namespace ui
} // namespace nx::vms::client::desktop
