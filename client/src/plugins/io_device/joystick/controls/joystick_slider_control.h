#pragma once

#include "base_joystick_control.h"

namespace nx {
namespace joystick {
namespace controls {

class Slider: public BaseControl
{

public:
    virtual StateAtom getSliderPosition() const;
};

typedef std::shared_ptr<Slider> SliderPtr;

} // namespace controls
} // namespace joystick
} // namespace nx