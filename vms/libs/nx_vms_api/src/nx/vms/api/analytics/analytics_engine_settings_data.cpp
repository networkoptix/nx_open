// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "analytics_engine_settings_data.h"

#include <nx/fusion/model_functions.h>

namespace nx::vms::api::analytics {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    EngineSettingsResponse,
    (json),
    nx_vms_api_analytics_EngineSettingsResponse_Fields,
    (brief, true))

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    EngineSettingsRequest,
    (json),
    nx_vms_api_analytics_EngineSettingsRequest_Fields,
    (brief, true))

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    EngineActiveSettingChangedResponse,
    (json),
    nx_vms_api_analytics_EngineActiveSettingChangedResponse_Fields,
    (brief, true))

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    EngineActiveSettingChangedRequest,
    (json),
    nx_vms_api_analytics_EngineActiveSettingChangedRequest_Fields,
    (brief, true))

} // namespace nx::vms::api::analytics
