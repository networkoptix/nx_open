#pragma once

#include <nx/gstreamer/bus_message.h>

namespace nx {
namespace gstreamer {


struct StateChangeInfo
{
    GstState oldState = GST_STATE_NULL;
    GstState newState = GST_STATE_NULL;
    GstState pending = GST_STATE_NULL;
};

class StateChangeBusMessage: public BusMessage
{
public:
    StateChangeInfo stateChangeInfo() const;
};

} // namespace gstreamer
} // namespace nx
