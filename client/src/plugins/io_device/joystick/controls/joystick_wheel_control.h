#pragma once

#include "base_joystick_control.h"

namespace nx {
namespace joystick {
namespace controls {

class Wheel: public BaseControl
{

public:
    virtual StateAtom getWheelRotation() const;
};

typedef std::shared_ptr<Wheel> WheelPtr;

} // namespace controls
} // namespace joystick
} // namespace nx