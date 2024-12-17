// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "event_log_v3.h"

#include <nx/fusion/model_functions.h>

namespace nx::vms::api::rules {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(EventParametersV3, (json)(ubjson), EventParametersV3_Fields,
    (brief, true))
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(ActionParametersV3, (json)(ubjson), ActionParametersV3_Fields,
    (brief, true))
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(EventLogRecordV3, (json)(ubjson), EventLogRecordV3_Fields)

} // namespace nx::vms::api::rules
