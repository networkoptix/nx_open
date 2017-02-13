#pragma once

namespace nx {
namespace joystick {

enum class ControlCapability 
{
    xAxisCapability = 0x0001,
    yAxisCapability = 0x0002,
    zAxisCapability = 0x0004,
    rxAxisCapability = 0x0008,
    ryAxisCapability = 0x0010,
    rzAxisCapability = 0x0020,

    colorChangeCapabilty = 0x0040,
};

enum class EventType 
{
    buttonDown = 1,
    buttonUp,
    buttonPressed,
    buttonDoublePressed,
    stickNonZero,
    stickZero,
    stickPushUp,
    stickPushDown,
    stickPushRight,
    stickPushLeft
};


typedef int64_t StateAtom;
typedef std::vector<StateAtom> State;
typedef QFlags<ControlCapability> ControlCapabilities;

struct EventParameters
{
    State state;
};

typedef std::function<void(State)> StateChangeHandler;
typedef std::function<void(EventType, EventParameters)> EventHandler;

} // namespace joystick
} // namespace nx