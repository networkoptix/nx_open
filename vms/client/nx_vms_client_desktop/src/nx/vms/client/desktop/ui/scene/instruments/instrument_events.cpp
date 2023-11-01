// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "instrument_events.h"

namespace nx::vms::client::desktop {
namespace ui {
namespace scene {

BaseEvent::BaseEvent()
{
    QQmlEngine::setObjectOwnership(this, QQmlEngine::CppOwnership);
}

SinglePointEvent::SinglePointEvent(const QSinglePointEvent* event):
    localPosition(event->position()),
    windowPosition(event->scenePosition()),
    screenPosition(event->globalPosition()),
    button(event->button()),
    buttons(event->buttons()),
    modifiers(event->modifiers())
{
}

MouseEvent::MouseEvent(const QMouseEvent* event):
    SinglePointEvent(event)
{
}

WheelEvent::WheelEvent(const QWheelEvent* event):
    SinglePointEvent(event),
    angleDelta(event->angleDelta()),
    pixelDelta(event->pixelDelta()),
    inverted(event->inverted())
{
}

HoverEvent::HoverEvent(const QHoverEvent* event):
    position(event->pos())
{
    QQmlEngine::setObjectOwnership(this, QQmlEngine::CppOwnership);
}

KeyEvent::KeyEvent(const QKeyEvent* event):
    key(event->key()),
    modifiers(event->modifiers())
{
    QQmlEngine::setObjectOwnership(this, QQmlEngine::CppOwnership);
}

} // namespace scene
} // namespace ui
} // namespace nx::vms::client::desktop
