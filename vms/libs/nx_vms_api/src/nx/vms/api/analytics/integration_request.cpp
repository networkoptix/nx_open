// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "integration_request.h"

#include <nx/fusion/model_functions.h>

namespace nx::vms::api::analytics {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(IntegrationRequestIdentity,
    (json),
    nx_vms_api_analytics_IntegrationRequestIdentity_Fields, (brief, true))

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(UpdateIntegrationRequest,
    (json),
    nx_vms_api_analytics_UpdateIntegrationRequest_Fields, (brief, true))

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(RegisterIntegrationResponse,
    (json),
    nx_vms_api_analytics_RegisterIntegrationResponse_Fields, (brief, true))

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(IntegrationRequestData,
    (json),
    nx_vms_api_analytics_IntegrationRequestData_Fields, (brief, true))

} // namespace nx::vms::api::analytics
