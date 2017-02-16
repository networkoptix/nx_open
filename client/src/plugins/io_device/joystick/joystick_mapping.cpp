#include "joystick_mapping.h"

#include <utils/common/model_functions.h>

namespace nx {
namespace joystick {
namespace mapping {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(Rule, (json), (ruleId)(eventType)(actionType)(actionParameters));
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(JoystickConfiguration, (json), (configurationId)(configurationName)(controlOverrides)(eventMapping));
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(Config, (json), (enabledConfigurations)(configurations));

} // namespace mapping
} // namespace joystick
} // namespace nx
