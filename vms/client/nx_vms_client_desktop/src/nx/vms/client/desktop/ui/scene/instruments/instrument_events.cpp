#include "instrument_events.h"

namespace nx::vms::client::desktop {
namespace ui {
namespace scene {

MouseEvent::MouseEvent(const QMouseEvent* event):
    position(event->pos()),
    globalPosition(event->globalPos()),
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
