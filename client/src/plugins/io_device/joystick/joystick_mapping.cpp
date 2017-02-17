#include "joystick_mapping.h"

#include <utils/common/model_functions.h>

namespace nx {
namespace joystick {
namespace mapping {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES((Rule), (json), _Fields);
QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES((JoystickConfiguration), (json), _Fields);
QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES((Config), (json), _Fields);

} // namespace mappings
} // namespace joystick
} // namespace nx
