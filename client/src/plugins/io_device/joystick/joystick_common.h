#pragma once

#include <utils/common/model_functions_fwd.h>

namespace nx {
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

enum class EventType 
{
    buttonDown = 1,
    buttonUp,
    buttonPressed,
    buttonDoublePressed,
    stickMove
};


QN_FUSION_DECLARE_FUNCTIONS(EventType, (lexical)(metatype))


typedef int64_t StateElement;
typedef std::vector<StateElement> State;
typedef QFlags<ControlCapability> ControlCapabilities;

struct EventParameters
{
    QString controlId;
    State state;
};

typedef std::function<void(EventType, EventParameters)> EventHandler;

} // namespace joystick
} // namespace nx




