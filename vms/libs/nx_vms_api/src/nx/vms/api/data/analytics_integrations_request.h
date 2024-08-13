// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/fusion/model_functions_fwd.h>
#include <nx/reflect/instrument.h>
#include <nx/utils/uuid.h>

namespace nx::vms::api::analytics {

struct NX_VMS_API AnalyticsIntegrationsRequest
{
    /**%apidoc[opt]
     * Analytics Integration id. Can be obtained from "id" field via `GET /rest/v{4-}/analytics/integrations`.
     */
    Uuid id;
};

#define nx_vms_api_analytics_AnalyticsIntegrationsRequest_Fields (id)
QN_FUSION_DECLARE_FUNCTIONS(AnalyticsIntegrationsRequest, (json), NX_VMS_API);
NX_REFLECTION_INSTRUMENT(AnalyticsIntegrationsRequest,
    nx_vms_api_analytics_AnalyticsIntegrationsRequest_Fields);

} // namespace nx::vms::api::analytics
