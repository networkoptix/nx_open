#include "joystick_led_control.h"

namespace {

const nx::client::plugins::io_device::joystick::State::size_type kLedStateSize = 1;

} // namespace

namespace nx {
namespace client {
namespace plugins {
namespace io_device {
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

boost::optional<Led::Color> Led::color() const
{
    QnMutexLocker lock(&m_mutex);
    return m_color;
}

void Led::setColor(Color color)
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
    return caps.testFlag(ControlCapability::colorChange);
}

} // namespace controls
} // namespace joystick
} // namespace io_device
} // namespace plugins
} // namespace client
} // namespace nx