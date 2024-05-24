// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "analytics_engines_filter.h"

#include <nx/fusion/model_functions.h>

namespace nx::vms::api::analytics {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(AnalyticsEnginesFilter, (json),
    nx_vms_api_analytics_AnalyticsEnginesFilter_Fields)

} // namespace nx::vms::api
