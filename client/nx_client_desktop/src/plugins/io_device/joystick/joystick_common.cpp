#include "joystick_common.h"
#include <nx/fusion/model_functions.h>

namespace nx {
namespace client {
namespace plugins {
namespace io_device {
namespace joystick {

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(nx::client::plugins::io_device::joystick, EventType,
    ((int)nx::client::plugins::io_device::joystick::EventType::buttonDown, "buttonDown")
    ((int)nx::client::plugins::io_device::joystick::EventType::buttonUp, "buttonUp")
    ((int)nx::client::plugins::io_device::joystick::EventType::buttonPressed, "buttonPressed")
    ((int)nx::client::plugins::io_device::joystick::EventType::buttonDoublePressed, "buttonDoublePressed")
    ((int)nx::client::plugins::io_device::joystick::EventType::stickMove, "stickMove")
)

} // namespace joystick
} // namespace io_device
} // namespace plugins
} // namespace client
} // namespace nx
