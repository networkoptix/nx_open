// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "object_action.h"

#include <nx/fusion/model_functions.h>

namespace nx::vms::api::analytics {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(ObjectActionRequirements, (json),
    nx_vms_api_analytics_Engine_ObjectAction_Requirements_Fields, (brief, true))

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(ObjectAction, (json),
    ObjectAction_Fields, (brief, true))

} // namespace nx::vms::api::analytics
