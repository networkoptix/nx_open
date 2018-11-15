#pragma once

#include <nx/gstreamer/bus_message.h>

namespace nx {
namespace gstreamer {

struct Error
{
    std::string message;
};

class ErrorBusMessage: public BusMessage
{
    Error errorMessage() const;
};

} // namespace gstreamer
} // namespace nx
