#pragma once

#include <nx/fusion/model_functions_fwd.h>

namespace nx {
namespace client {
namespace plugins {
namespace io_device {
namespace joystick {

enum class ControlCapability
{
    xAxis = 0x0001,
    yAxis = 0x0002,
    zAxis = 0x0004,
    rxAxis = 0x0008,
    ryAxis = 0x0010,
    rzAxis = 0x0020,

    colorChange = 0x0040,
};

Q_DECLARE_FLAGS(ControlCapabilities, ControlCapability);

enum class EventType
{
    buttonDown = 1,
    buttonUp,
    buttonPressed,
    buttonDoublePressed,
    stickMove
};

QN_FUSION_DECLARE_FUNCTIONS(EventType, (lexical))

typedef int64_t StateElement;
typedef std::vector<StateElement> State;

struct EventParameters
{
    QString controlId;
    State state;
};

typedef std::function<void(EventType, EventParameters)> EventHandler;

} // namespace joystick
} // namespace io_device
} // namespace plugins
} // namespace client
} // namespace nx




