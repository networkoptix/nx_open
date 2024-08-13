// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/fusion/model_functions_fwd.h>
#include <nx/reflect/instrument.h>
#include <nx/utils/uuid.h>
#include <nx/vms/api/json/value_or_array.h>

namespace nx::vms::api::analytics {

struct NX_VMS_API AnalyticsEnginesFilter
{
    /**%apidoc[opt]
     * Analytics Engine id. Can be obtained from "id" field via `GET /rest/v{4-}/analytics/engines`.
     */
    Uuid id;

    /**%apidoc[opt]:uuidArray
     * Integration id. Can be specified to get the Engine id of an Integration.
     */
    std::optional<nx::vms::api::json::ValueOrArray<Uuid>> integrationId;
};

#define nx_vms_api_analytics_AnalyticsEnginesFilter_Fields (id)(integrationId)
QN_FUSION_DECLARE_FUNCTIONS(AnalyticsEnginesFilter, (json), NX_VMS_API);
NX_REFLECTION_INSTRUMENT(AnalyticsEnginesFilter,
    nx_vms_api_analytics_AnalyticsEnginesFilter_Fields);

} // namespace nx::vms::api::analytics
