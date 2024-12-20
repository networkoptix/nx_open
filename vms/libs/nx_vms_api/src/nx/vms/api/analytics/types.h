// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <string>

#include <QtCore/QRectF>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/fusion/serialization/json_functions.h>
#include <nx/fusion/serialization/json_fwd.h>
#include <nx/reflect/instrument.h>
#include <nx/reflect/json.h>
#include <nx/utils/json/qjson.h>
#include <nx/utils/json/qt_core_types.h>
#include <nx/utils/uuid.h>
#include <nx/vms/api/analytics/device_agent_manifest.h>
#include <nx/vms/api/analytics/engine_manifest.h>
#include <nx/vms/api/analytics/integration_manifest.h>
#include <nx/vms/api/analytics/manifest_items.h>

namespace nx::vms::api::analytics {

/**%apidoc
 * Bounding box rectangle.
 */
struct NX_VMS_API Rect
{
    /**%apidoc[opt] X */
    float x = 0;
    /**%apidoc[opt] Y */
    float y = 0;
    /**%apidoc[opt] Width */
    float width = 0;
    /**%apidoc[opt] Height */
    float height = 0;

    bool operator==(const Rect& other) const = default;
};
#define nx_vms_api_analytics_Rect_Fields (x)(y)(width)(height)
QN_FUSION_DECLARE_FUNCTIONS(Rect, (json), NX_VMS_API);
NX_REFLECTION_INSTRUMENT(Rect, nx_vms_api_analytics_Rect_Fields);

NX_REFLECTION_ENUM_CLASS(Flags,
    none,
    cameraClockTimestamp
);

/**%apidoc
 * Data to update Engine manifest.
 */
struct NX_VMS_API PushEngineManifestData
{
    /**%apidoc Engine id */
    nx::Uuid id;

    /**%apidoc Engine Manifest */
    EngineManifest engineManifest;

    bool operator==(const PushEngineManifestData& other) const = default;
};
#define nx_vms_api_analytics_PushEngineManifestData_Fields \
    (id) \
    (engineManifest)
QN_FUSION_DECLARE_FUNCTIONS(
    PushEngineManifestData, (json), NX_VMS_API);
NX_REFLECTION_INSTRUMENT(
    PushEngineManifestData,
    nx_vms_api_analytics_PushEngineManifestData_Fields);

/**%apidoc
 * Data to update Device Agent manifest.
 */
struct NX_VMS_API PushDeviceAgentManifestData
{
    /**%apidoc Engine id */
    nx::Uuid id;

    /**%apidoc Device Agent id */
    nx::Uuid deviceId;

    /**%apidoc Device Agent manifest */
    DeviceAgentManifest deviceAgentManifest;

    bool operator==(const PushDeviceAgentManifestData& other) const = default;
};
#define nx_vms_api_analytics_PushDeviceAgentManifestData_Fields \
    (id) \
    (deviceId) \
    (deviceAgentManifest)
QN_FUSION_DECLARE_FUNCTIONS(
    PushDeviceAgentManifestData, (json), NX_VMS_API);
NX_REFLECTION_INSTRUMENT(
    PushDeviceAgentManifestData,
    nx_vms_api_analytics_PushDeviceAgentManifestData_Fields);

NX_REFLECTION_ENUM_CLASS(IntegrationDiagnosticEventLevel,
    info,
    warning,
    error
);

/**%apidoc
 * Integration Diagnostic Event from Engine.
 */
struct NX_VMS_API EngineIntegrationDiagnosticEvent
{
    /**%apidoc Engine id */
    nx::Uuid id;
    /**%apidoc Level */
    IntegrationDiagnosticEventLevel level;
    /**%apidoc Caption */
    std::string caption;
    /**%apidoc Description */
    std::string description;

    bool operator==(const EngineIntegrationDiagnosticEvent& other) const = default;
};
#define nx_vms_api_analytics_EngineIntegrationDiagnosticEvent_Fields \
    (id)(level)(caption)(description)

QN_FUSION_DECLARE_FUNCTIONS(EngineIntegrationDiagnosticEvent, (json), NX_VMS_API);
NX_REFLECTION_INSTRUMENT(EngineIntegrationDiagnosticEvent,
    nx_vms_api_analytics_EngineIntegrationDiagnosticEvent_Fields);

/**%apidoc
 *  Integration Diagnostic Event from Device Agent.
 */
struct NX_VMS_API DeviceAgentIntegrationDiagnosticEvent
{
    /**%apidoc Engine id */
    nx::Uuid id;
    /**%apidoc Device Agent id */
    nx::Uuid deviceId;
    /**%apidoc Level */
    IntegrationDiagnosticEventLevel level;
    /**%apidoc Caption */
    std::string caption;
    /**%apidoc Description */
    std::string description;

    bool operator==(const DeviceAgentIntegrationDiagnosticEvent& other) const = default;
};
#define nx_vms_api_analytics_DeviceAgentIntegrationDiagnosticEvent_Fields \
    (id)(deviceId)(level)(caption)(description)

QN_FUSION_DECLARE_FUNCTIONS(DeviceAgentIntegrationDiagnosticEvent, (json), NX_VMS_API);
NX_REFLECTION_INSTRUMENT(DeviceAgentIntegrationDiagnosticEvent,
    nx_vms_api_analytics_DeviceAgentIntegrationDiagnosticEvent_Fields);

/**%apidoc
 * Metadata attribute.
 */
struct NX_VMS_API ApiObjectMetadataAttribute
{
    /**%apidoc[opt] Type */
    AttributeType type = AttributeType::undefined;
    /**%apidoc[opt] Name */
    std::string name;
    /**%apidoc[opt] Value */
    std::string value;
    /**%apidoc[opt] Confidence */
    float confidence = 1.0;

    bool operator==(const ApiObjectMetadataAttribute& other) const = default;
};
#define nx_vms_api_analytics_ApiObjectMetadataAttribute_Fields \
    (type)(name)(value)(confidence)
QN_FUSION_DECLARE_FUNCTIONS(ApiObjectMetadataAttribute, (json));
NX_REFLECTION_INSTRUMENT(ApiObjectMetadataAttribute,
    nx_vms_api_analytics_ApiObjectMetadataAttribute_Fields);

/**%apidoc
 * Object Metadata.
 */
struct NX_VMS_API ApiObjectMetadata
{
    /**%apidoc[opt] Track id */
    Uuid trackId;
    /**%apidoc[opt] Subtype */
    std::string subtype;
    /**%apidoc[opt] Bounding box */
    Rect boundingBox = {0, 0, 0, 0};
    /**%apidoc[opt] Type id */
    std::string typeId;
    /**%apidoc[opt] Confidence */
    float confidence = 1.0;
    /**%apidoc[opt] Attributes */
    std::vector<ApiObjectMetadataAttribute> attributes;

    bool operator==(const ApiObjectMetadata& other) const = default;
};
#define nx_vms_api_analytics_ApiObjectMetadata_Fields \
    (trackId)(subtype)(boundingBox)(typeId)(confidence)(attributes)
QN_FUSION_DECLARE_FUNCTIONS(ApiObjectMetadata, (json));
NX_REFLECTION_INSTRUMENT(ApiObjectMetadata,
    nx_vms_api_analytics_ApiObjectMetadata_Fields);

/**%apidoc
 * Object Metadata packet.
 */
struct NX_VMS_API ApiObjectMetadataPacket
{
    /**%apidoc Engine id */
    nx::Uuid id;
    /**%apidoc Device Agent id */
    nx::Uuid deviceId;
    /**%apidoc[opt] Flags */
    Flags flags = Flags::none;
    /**%apidoc[opt] Timestamp */
    std::chrono::microseconds timestampUs;
    /**%apidoc[opt] Duration */
    std::chrono::microseconds durationUs;
    /**%apidoc[opt] Objects */
    std::vector<ApiObjectMetadata> objects;

    bool operator==(const ApiObjectMetadataPacket& other) const = default;
};
#define nx_vms_api_analytics_ApiObjectMetadataPacket_Fields \
    (id)(deviceId)(flags)(timestampUs)(durationUs)(objects)
QN_FUSION_DECLARE_FUNCTIONS(ApiObjectMetadataPacket, (json), NX_VMS_API);
NX_REFLECTION_INSTRUMENT(ApiObjectMetadataPacket,
    nx_vms_api_analytics_ApiObjectMetadataPacket_Fields);

/**%apidoc
 * Best Shot metadata packet.
 */
struct NX_VMS_API ApiBestShotMetadataPacket
{
    /**%apidoc Engine id */
    nx::Uuid id;
    /**%apidoc Device Agent id */
    nx::Uuid deviceId;
    /**%apidoc[opt] Track id */
    Uuid trackId;
    /**%apidoc[opt] Flags */
    Flags flags = Flags::none;
    /**%apidoc[opt] Timestamp */
    std::chrono::microseconds timestampUs;
    /**%apidoc[opt] Bounding box */
    Rect boundingBox = {0, 0, 0, 0};
    /**%apidoc[opt] Image URL */
    std::string imageUrl;
    /**%apidoc[opt] Image data */
    std::vector<char> imageData;
    /**%apidoc[opt] Image data format */
    std::string imageDataFormat;
    /**%apidoc[opt] Attributes */
    std::vector<ApiObjectMetadataAttribute> attributes;

    bool operator==(const ApiBestShotMetadataPacket& other) const = default;
};
#define nx_vms_api_analytics_ApiBestShotMetadataPacket_Fields \
    (id)(deviceId)(trackId)(flags)(timestampUs)(boundingBox) \
    (imageUrl)(imageData)(imageDataFormat)(attributes)
QN_FUSION_DECLARE_FUNCTIONS(ApiBestShotMetadataPacket, (json), NX_VMS_API);
NX_REFLECTION_INSTRUMENT(ApiBestShotMetadataPacket,
    nx_vms_api_analytics_ApiBestShotMetadataPacket_Fields);

/**%apidoc
 * Object Title metadata packet.
 */
struct NX_VMS_API ApiObjectTitleMetadataPacket
{
    /**%apidoc Engine id */
    nx::Uuid id;
    /**%apidoc Device Agent id */
    nx::Uuid deviceId;
    /**%apidoc[opt] Track id */
    Uuid trackId;
    /**%apidoc[opt] Flags */
    Flags flags = Flags::none;
    /**%apidoc[opt] Timestamp */
    std::chrono::microseconds timestampUs;
    /**%apidoc[opt] Bounding box */
    Rect boundingBox = {0, 0, 0, 0};
    /**%apidoc[opt] Text */
    std::string text;
    /**%apidoc[opt] Image URL */
    std::string imageUrl;
    /**%apidoc[opt] Image data */
    std::vector<char> imageData;
    /**%apidoc[opt] Image data format */
    std::string imageDataFormat;

    bool operator==(const ApiObjectTitleMetadataPacket& other) const = default;
};
#define nx_vms_api_analytics_ApiObjectTitleMetadataPacket_Fields \
    (id)(deviceId)(trackId)(flags)(timestampUs)(boundingBox) \
    (text)(imageUrl)(imageData)(imageDataFormat)
QN_FUSION_DECLARE_FUNCTIONS(ApiObjectTitleMetadataPacket, (json), NX_VMS_API);
NX_REFLECTION_INSTRUMENT(ApiObjectTitleMetadataPacket,
    nx_vms_api_analytics_ApiObjectTitleMetadataPacket_Fields);

/**%apidoc
 * Event metadata.
 */
struct NX_VMS_API ApiEventMetadata
{
    /**%apidoc[opt] Type id */
    std::string typeId;
    /**%apidoc[opt] Caption */
    std::string caption;
    /**%apidoc[opt] Description */
    std::string description;
    /**%apidoc[opt] Is active */
    bool isActive = false;
    /**%apidoc[opt] Track id */
    Uuid trackId;
    /**%apidoc[opt] Key */
    std::string key;

    bool operator==(const ApiEventMetadata& other) const = default;
};
#define nx_vms_api_analytics_ApiEventMetadata_Fields \
    (typeId)(caption)(description)(isActive)(trackId)(key)
QN_FUSION_DECLARE_FUNCTIONS(ApiEventMetadata, (json));
NX_REFLECTION_INSTRUMENT(ApiEventMetadata,
    nx_vms_api_analytics_ApiEventMetadata_Fields);

/**%apidoc
 * Event metadata packet.
 */
struct NX_VMS_API ApiEventMetadataPacket
{
    /**%apidoc Engine id */
    nx::Uuid id;
    /**%apidoc Device Agent id */
    nx::Uuid deviceId;
    /**%apidoc[opt] Flags */
    Flags flags = Flags::none;
    /**%apidoc[opt] Timestamp */
    std::chrono::microseconds timestampUs;
    /**%apidoc[opt] Duration */
    std::chrono::microseconds durationUs;
    /**%apidoc[opt] Events */
    std::vector<ApiEventMetadata> events;

    bool operator==(const ApiEventMetadataPacket& other) const = default;
};
#define nx_vms_api_analytics_ApiEventMetadataPacket_Fields \
    (id)(deviceId)(flags)(timestampUs)(durationUs)(events)
QN_FUSION_DECLARE_FUNCTIONS(ApiEventMetadataPacket, (json), NX_VMS_API);
NX_REFLECTION_INSTRUMENT(ApiEventMetadataPacket,
    nx_vms_api_analytics_ApiEventMetadataPacket_Fields);

} // namespace nx::vms::api::analytics
