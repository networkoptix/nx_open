#include "instrument_events.h"

namespace nx {
namespace client {
namespace desktop {
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

} // namespace scene
} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
