#pragma once

#include "base_joystick_control.h"

namespace nx {
namespace joystick {
namespace controls {

class Led: public BaseControl
{
public:
    typedef uint32_t Color;

    virtual bool getLedState() const;
    virtual Color getLedColor() const;
    virtual void setLedColor(Color color);
    virtual bool canChangeColor() const;


};

typedef std::shared_ptr<Led> LedPtr;

} // namespace controls
} // namespace joystick
} // namespace nx

