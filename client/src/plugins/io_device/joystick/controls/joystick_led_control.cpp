#include "joystick_led_control.h"

namespace nx {
namespace joystick {
namespace controls {

bool Led::getLedState() const
{
    return false;
}

Led::Color Led::getLedColor() const
{
    return 0;
}

void Led::setLedColor(Color color)
{

}

bool Led::canChangeColor() const
{
    return false;
}

} // namespace controls
} // namespace joystick
} // namespace nx