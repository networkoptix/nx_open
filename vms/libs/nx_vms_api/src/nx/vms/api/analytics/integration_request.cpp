// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "integration_request.h"

#include <nx/fusion/model_functions.h>

namespace nx::vms::api::analytics {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(ApiIntegrationManifest,
    (json)(ubjson),
    nx_vms_api_analytics_ApiIntegrationManifest_Fields, (brief, true))

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(IntegrationRequestIdentity,
    (json)(ubjson),
    nx_vms_api_analytics_IntegrationRequestIdentity_Fields, (brief, true))

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(RegisterIntegrationRequest,
    (json)(ubjson),
    nx_vms_api_analytics_RegisterIntegrationRequest_Fields, (brief, true))

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(RegisterIntegrationResponse,
    (json)(ubjson),
    nx_vms_api_analytics_RegisterIntegrationResponse_Fields, (brief, true))

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(IntegrationRequestData,
    (json)(ubjson),
    nx_vms_api_analytics_IntegrationRequestData_Fields, (brief, true))

} // namespace nx::vms::api::analytics
