#include "joystick_config.h"

#include <nx/fusion/model_functions.h>

namespace nx {
namespace client {
namespace plugins {
namespace io_device {
namespace joystick {
namespace config {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES((Rule), (json), _Fields);
QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES((Configuration), (json), _Fields);
QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES((Config), (json), _Fields);

} // namespace config
} // namespace joystick
} // namespace io_device
} // namespace plugins
} // namespace client
} // namespace nx
