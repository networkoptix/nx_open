#include "event_info.h"

#include <nx/fusion/model_functions.h>

namespace nx::vms::api::rules {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(EventInfo, (json),
    nx_vms_api_rules_EventInfo_Fields, (brief, true))

} // namespace nx::vms::api::rules
