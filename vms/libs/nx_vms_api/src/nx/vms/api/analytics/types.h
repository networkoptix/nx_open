// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QRectF>
#include <QtCore/QString>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/utils/uuid.h>

namespace nx::vms::api::analytics {

struct NX_VMS_API EngineIdentity
{
    QString engineId;
};
#define nx_vms_api_analytics_EngineIdentity_Fields (engineId)
QN_FUSION_DECLARE_FUNCTIONS(EngineIdentity, (json), NX_VMS_API)

struct NX_VMS_API DeviceAgentIdentity
{
    QString engineId;
    QString deviceId;
};
#define nx_vms_api_analytics_DeviceAgentIdentity_Fields (engineId)(deviceId)
QN_FUSION_DECLARE_FUNCTIONS(DeviceAgentIdentity, (json), NX_VMS_API)

NX_VMS_API bool operator<(const DeviceAgentIdentity& first, const DeviceAgentIdentity& second);

struct NX_VMS_API ObtainEngineRequestParameters
{
    QString engineId;
    QString engineName;
};
#define nx_vms_api_analytics_ObtainEngineRequestParameters_Fields (engineId)(engineName)
QN_FUSION_DECLARE_FUNCTIONS(ObtainEngineRequestParameters, (json), NX_VMS_API)

struct NX_VMS_API ObtainDeviceAgentRequestParameters
{
    QString deviceId;
};
#define nx_vms_api_analytics_ObtainDeviceAgentRequestParameters_Fields (deviceId)
QN_FUSION_DECLARE_FUNCTIONS(ObtainDeviceAgentRequestParameters, (json), NX_VMS_API)

struct NX_VMS_API RpcObjectMetadataAttribute
{
    QString name;
    QString value;
};
#define nx_vms_api_analytics_RpcObjectMetadataAttribute_Fields (name)(value)
QN_FUSION_DECLARE_FUNCTIONS(RpcObjectMetadataAttribute, (json), NX_VMS_API)

struct Rect
{
    float x = 0;
    float y = 0;
    float width = 0;
    float height = 0;
};
#define nx_vms_api_analytics_Rect_Fields (x)(y)(width)(height)
QN_FUSION_DECLARE_FUNCTIONS(Rect, (json), NX_VMS_API)

struct NX_VMS_API RpcObjectMetadata
{
    nx::Uuid trackId;
    QString typeId;
    Rect boundingBox;
    std::vector<RpcObjectMetadataAttribute> attributes;
};
#define nx_vms_api_analytics_RpcObjectMetadata_Fields (trackId)(typeId)(boundingBox)(attributes)
QN_FUSION_DECLARE_FUNCTIONS(RpcObjectMetadata, (json), NX_VMS_API)

struct NX_VMS_API RpcObjectMetadataPacket
{
    std::chrono::microseconds timestampUs;
    std::chrono::microseconds durationUs;
    std::vector<RpcObjectMetadata> metadata;
};
#define nx_vms_api_analytics_RpcObjectMetadataPacket_Fields (timestampUs)(durationUs)(metadata)
QN_FUSION_DECLARE_FUNCTIONS(RpcObjectMetadataPacket, (json), NX_VMS_API)

struct NX_VMS_API DataPacket
{
    // TODO: #dmishin Either remove this struct or add other fields.
    std::chrono::microseconds timestampUs;
};
#define nx_vms_api_analytics_DataPacket_Fields (timestampUs)
QN_FUSION_DECLARE_FUNCTIONS(DataPacket, (json), NX_VMS_API)

} // namespace nx::vms::api::analytics
