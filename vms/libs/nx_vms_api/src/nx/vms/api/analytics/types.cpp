// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "types.h"

#include <nx/fusion/model_functions.h>

namespace nx::vms::api::analytics {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(Rect, (json),
    nx_vms_api_analytics_Rect_Fields, (brief, true))

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(PushEngineManifestData,
    (json),
    nx_vms_api_analytics_PushEngineManifestData_Fields, (brief, true))

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(PushDeviceAgentManifestData,
    (json),
    nx_vms_api_analytics_PushDeviceAgentManifestData_Fields, (brief, true))

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(EngineIntegrationDiagnosticEvent,
    (json),
    nx_vms_api_analytics_EngineIntegrationDiagnosticEvent_Fields, (brief, true))

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(DeviceAgentIntegrationDiagnosticEvent,
    (json),
    nx_vms_api_analytics_DeviceAgentIntegrationDiagnosticEvent_Fields, (brief, true))

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(ApiObjectMetadataAttribute, (json),
    nx_vms_api_analytics_ApiObjectMetadataAttribute_Fields, (brief, true))

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(ApiObjectMetadata, (json),
    nx_vms_api_analytics_ApiObjectMetadata_Fields, (brief, true))

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(ApiObjectMetadataPacket, (json),
    nx_vms_api_analytics_ApiObjectMetadataPacket_Fields, (brief, true))

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(ApiBestShotMetadataPacket, (json),
    nx_vms_api_analytics_ApiBestShotMetadataPacket_Fields, (brief, true))

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(ApiObjectTitleMetadataPacket, (json),
    nx_vms_api_analytics_ApiObjectTitleMetadataPacket_Fields, (brief, true))

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(ApiEventMetadata, (json),
    nx_vms_api_analytics_ApiEventMetadata_Fields, (brief, true))

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(ApiEventMetadataPacket, (json),
    nx_vms_api_analytics_ApiEventMetadataPacket_Fields, (brief, true))

} // namespace nx::vms::api::analytics
