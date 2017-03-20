#pragma once

#include "base_joystick_control.h"
#include <boost/optional/optional.hpp>

namespace nx {
namespace client {
namespace plugins {
namespace io_device {
namespace joystick {
namespace controls {

class Led: public BaseControl
{
    typedef BaseControl base_type;
public:
    Led();

    typedef uint32_t Color;

    virtual bool getLedState() const;
    virtual boost::optional<Color> color() const;
    virtual void setColor(Color color);
    virtual bool canChangeColor() const;

private:
    boost::optional<Led::Color> m_color;
};

typedef std::shared_ptr<Led> LedPtr;

} // namespace controls
} // namespace joystick
} // namespace io_device
} // namespace plugins
} // namespace client
} // namespace nx

