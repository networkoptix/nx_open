// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "types.h"

#include <nx/fusion/model_functions.h>

namespace nx::vms::api::analytics {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(Rect, (json),
    nx_vms_api_analytics_Rect_Fields, (brief, true))

} // namespace nx::vms::api::analytics
