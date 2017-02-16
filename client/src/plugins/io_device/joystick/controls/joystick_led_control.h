#pragma once

#include "base_joystick_control.h"
#include <boost/optional/optional.hpp>

namespace nx {
namespace joystick {
namespace controls {

class Led: public BaseControl
{
    typedef BaseControl base_type;
public:
    Led();

    typedef uint32_t Color;

    virtual bool getLedState() const;
    virtual boost::optional<Color> getLedColor() const;
    virtual void setLedColor(Color color);
    virtual bool canChangeColor() const;

private:
    boost::optional<Led::Color> m_color;
};

typedef std::shared_ptr<Led> LedPtr;

} // namespace controls
} // namespace joystick
} // namespace nx

