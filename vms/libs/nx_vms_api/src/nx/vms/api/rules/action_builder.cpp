#include "action_builder.h"

#include <nx/fusion/model_functions.h>

namespace nx::vms::api::rules {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(ActionBuilder, (json),
    nx_vms_api_rules_ActionBuilder_Fields, (brief, true))

} // namespace nx::vms::api::rules
