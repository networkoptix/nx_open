#include "joystick_common.h"
#include <utils/common/model_functions.h>

namespace nx {
namespace joystick {

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(nx::joystick, EventType,
    ((int)nx::joystick::EventType::buttonDown, "buttonDown")
    ((int)nx::joystick::EventType::buttonUp, "buttonUp")
    ((int)nx::joystick::EventType::buttonPressed, "buttonPressed")
    ((int)nx::joystick::EventType::stickMove, "stickMove")
)

} // namespace joystick
} // namespace nx
