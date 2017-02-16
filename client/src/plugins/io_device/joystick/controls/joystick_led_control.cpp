#include "joystick_led_control.h"

namespace {
    const nx::joystick::State::size_type kLedStateSize = 1;
} // namespace

namespace nx {
namespace joystick {
namespace controls {

Led::Led():
    base_type(kLedStateSize),
    m_color(boost::none)
{

}

bool Led::getLedState() const
{
    QnMutexLocker lock(&m_mutex);
    return m_state[0];
}

boost::optional<Led::Color> Led::getLedColor() const
{
    QnMutexLocker lock(&m_mutex);
    return m_color;
}

void Led::setLedColor(Color color)
{
    auto colorChangeAllowed = canChangeColor();
    if (!colorChangeAllowed)
        return;

    QnMutexLocker lock(&m_mutex);
    m_color = color;
}

bool Led::canChangeColor() const
{
    auto caps = getCapabilities();
    return caps & nx::joystick::ControlCapability::colorChange;
}

} // namespace controls
} // namespace joystick
} // namespace nx