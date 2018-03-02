#include "state_change_bus_message.h"

namespace nx {
namespace gstreamer {

StateChangeInfo StateChangeBusMessage::stateChangeInfo() const
{
    StateChangeInfo info;
    gst_message_parse_state_changed(
        m_message.get(),
        &info.oldState,
        &info.newState,
        &info.pending);

    return info;
}

} // namespace gstreamer
} // namespace nx

