#include "event_filter.h"

#include <nx/fusion/model_functions.h>

namespace nx::vms::api::rules {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(EventFilter, (json),
    nx_vms_api_rules_EventFilter_Fields, (brief, true))

} // namespace nx::vms::api::rules
