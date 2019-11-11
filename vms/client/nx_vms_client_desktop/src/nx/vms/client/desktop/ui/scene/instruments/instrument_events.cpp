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

} // namespace scene
} // namespace ui
} // namespace nx::vms::client::desktop
