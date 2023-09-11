// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "types.h"

#include <nx/fusion/model_functions.h>

namespace nx::vms::api::analytics {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(EngineIdentity, (json),
    nx_vms_api_analytics_EngineIdentity_Fields, (brief, true))

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(DeviceAgentIdentity, (json),
    nx_vms_api_analytics_DeviceAgentIdentity_Fields, (brief, true))

bool operator<(const DeviceAgentIdentity& first, const DeviceAgentIdentity& second)
{
    if (first.engineId != second.engineId)
        return first.engineId < second.engineId;

    if (first.deviceId != second.deviceId)
        return first.deviceId < second.deviceId;

    return false;
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(ObtainEngineRequestParameters, (json),
    nx_vms_api_analytics_ObtainEngineRequestParameters_Fields, (brief, true))

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(ObtainDeviceAgentRequestParameters, (json),
    nx_vms_api_analytics_ObtainDeviceAgentRequestParameters_Fields, (brief, true))

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(RpcObjectMetadataAttribute, (json),
    nx_vms_api_analytics_RpcObjectMetadataAttribute_Fields, (brief, true))

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(Rect, (json),
    nx_vms_api_analytics_Rect_Fields, (brief, true))

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(RpcObjectMetadata, (json),
    nx_vms_api_analytics_RpcObjectMetadata_Fields, (brief, true))

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(RpcObjectMetadataPacket, (json),
    nx_vms_api_analytics_RpcObjectMetadataPacket_Fields, (brief, true))

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(DataPacket, (json),
    nx_vms_api_analytics_DataPacket_Fields, (brief, true))

} // namespace nx::vms::api::analytics
