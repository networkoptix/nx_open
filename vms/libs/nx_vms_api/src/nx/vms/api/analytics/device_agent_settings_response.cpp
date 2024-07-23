// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "device_agent_settings_response.h"

#include <nx/fusion/model_functions.h>

namespace nx::vms::api::analytics {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    DeviceAgentSettingsResponse,
    (json),
    nx_vms_api_analytics_DeviceAgentSettingsResponse_Fields,
    (brief, true))

} // namespace nx::vms::api::analytics
