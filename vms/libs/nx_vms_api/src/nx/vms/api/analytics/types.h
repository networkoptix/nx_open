// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <string>

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
#include <nx/vms/api/data/rect_as_string.h>
#include <nx/vms/api/json/value_or_array.h>

namespace nx::vms::api::analytics {

NX_REFLECTION_ENUM_CLASS(Flags,
    none,
    cameraClockTimestamp
);

/**%apidoc
 * Data to update Engine manifest.
 */
struct NX_VMS_API PushEngineManifestData
{
    /**%apidoc Engine id. */
    nx::Uuid id;

    EngineManifest engineManifest;

    bool operator==(const PushEngineManifestData& other) const = default;
};
#define nx_vms_api_analytics_PushEngineManifestData_Fields \
    (id) \
    (engineManifest)
QN_FUSION_DECLARE_FUNCTIONS(
    PushEngineManifestData, (json), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(
    PushEngineManifestData,
    nx_vms_api_analytics_PushEngineManifestData_Fields)

/**%apidoc
 * Data to update Device Agent manifest.
 */
struct NX_VMS_API PushDeviceAgentManifestData
{
    nx::Uuid engineId;

    /**%apidoc
     * Device id (can be obtained from "id", "physicalId" or "logicalId" field via
     * `GET /rest/v{1-}/devices`) or MAC address (not supported for certain cameras).
     */
    QString deviceId;

    DeviceAgentManifest deviceAgentManifest;

    bool operator==(const PushDeviceAgentManifestData& other) const = default;
};
#define nx_vms_api_analytics_PushDeviceAgentManifestData_Fields \
    (engineId) \
    (deviceId) \
    (deviceAgentManifest)
QN_FUSION_DECLARE_FUNCTIONS(
    PushDeviceAgentManifestData, (json), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(
    PushDeviceAgentManifestData,
    nx_vms_api_analytics_PushDeviceAgentManifestData_Fields)

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

QN_FUSION_DECLARE_FUNCTIONS(EngineIntegrationDiagnosticEvent, (json), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(EngineIntegrationDiagnosticEvent,
    nx_vms_api_analytics_EngineIntegrationDiagnosticEvent_Fields)

/**%apidoc
 *  Integration Diagnostic Event from Device Agent.
 */
struct NX_VMS_API DeviceAgentIntegrationDiagnosticEvent
{
    /**%apidoc Engine id */
    nx::Uuid id;

    /**%apidoc
     * Device id (can be obtained from "id", "physicalId" or "logicalId" field via
     * `GET /rest/v{1-}/devices`) or MAC address (not supported for certain cameras).
     */
    QString deviceId;
    IntegrationDiagnosticEventLevel level;
    std::string caption;
    std::string description;

    bool operator==(const DeviceAgentIntegrationDiagnosticEvent& other) const = default;
};
#define nx_vms_api_analytics_DeviceAgentIntegrationDiagnosticEvent_Fields \
    (id)(deviceId)(level)(caption)(description)

QN_FUSION_DECLARE_FUNCTIONS(DeviceAgentIntegrationDiagnosticEvent, (json), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(DeviceAgentIntegrationDiagnosticEvent,
    nx_vms_api_analytics_DeviceAgentIntegrationDiagnosticEvent_Fields)

/**%apidoc
 * Metadata attribute.
 */
struct NX_VMS_API ApiObjectMetadataAttribute
{
    /**%apidoc[opt] */
    AttributeType type = AttributeType::undefined;

    /**%apidoc[opt] */
    std::string name;

    /**%apidoc[opt] */
    std::string value;

    /**%apidoc[opt] */
    float confidence = 1.0;

    bool operator==(const ApiObjectMetadataAttribute& other) const = default;
};
#define nx_vms_api_analytics_ApiObjectMetadataAttribute_Fields \
    (type)(name)(value)(confidence)
QN_FUSION_DECLARE_FUNCTIONS(ApiObjectMetadataAttribute, (json))
NX_REFLECTION_INSTRUMENT(ApiObjectMetadataAttribute,
    nx_vms_api_analytics_ApiObjectMetadataAttribute_Fields)

/**%apidoc
 * Object Metadata.
 */
struct NX_VMS_API ApiObjectMetadata
{
    /**%apidoc[opt] */
    Uuid trackId;

    /**%apidoc[opt] */
    std::string subtype;

    /**%apidoc[opt]:string
     * Coordinates of the bounding box on a video frame where an Object is shown; in range [0..1].
     * The format is: `{x},{y},{width}x{height}`
     */
    RectAsString boundingBox = {0, 0, 0, 0};

    /**%apidoc[opt]:string */
    std::string typeId;

    /**%apidoc[opt] */
    float confidence = 1.0;

    /**%apidoc[opt] */
    std::vector<ApiObjectMetadataAttribute> attributes;

    bool operator==(const ApiObjectMetadata& other) const = default;
};
#define nx_vms_api_analytics_ApiObjectMetadata_Fields \
    (trackId)(subtype)(boundingBox)(typeId)(confidence)(attributes)
QN_FUSION_DECLARE_FUNCTIONS(ApiObjectMetadata, (json))
NX_REFLECTION_INSTRUMENT(ApiObjectMetadata,
    nx_vms_api_analytics_ApiObjectMetadata_Fields)

/**%apidoc
 * Object Metadata packet.
 */
struct NX_VMS_API ApiObjectMetadataPacket
{
    /**%apidoc Engine id */
    nx::Uuid id;

    /**%apidoc
     * Device id (can be obtained from "id", "physicalId" or "logicalId" field via
     * `GET /rest/v{1-}/devices`) or MAC address (not supported for certain cameras).
     */
    QString deviceId;

    /**%apidoc[opt] */
    Flags flags = Flags::none;

    /**%apidoc[opt] */
    std::chrono::milliseconds timestampMs;

    /**%apidoc[opt] */
    std::chrono::milliseconds durationMs;

    /**%apidoc[opt] */
    std::vector<ApiObjectMetadata> objects;

    bool operator==(const ApiObjectMetadataPacket& other) const = default;
};
#define nx_vms_api_analytics_ApiObjectMetadataPacket_Fields \
    (id)(deviceId)(flags)(timestampMs)(durationMs)(objects)
QN_FUSION_DECLARE_FUNCTIONS(ApiObjectMetadataPacket, (json), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(ApiObjectMetadataPacket,
    nx_vms_api_analytics_ApiObjectMetadataPacket_Fields)

/**%apidoc
 * Best Shot metadata packet.
 */
struct NX_VMS_API ApiBestShotMetadataPacket
{
    /**%apidoc Engine id */
    nx::Uuid id;

    /**%apidoc
     * Device id (can be obtained from "id", "physicalId" or "logicalId" field via
     * `GET /rest/v{1-}/devices`) or MAC address (not supported for certain cameras).
     */
    QString deviceId;

    /**%apidoc[opt] */
    nx::Uuid trackId;

    /**%apidoc[opt] */
    Flags flags = Flags::none;

    /**%apidoc[opt] */
    std::chrono::milliseconds timestampMs;
    /**%apidoc[opt]:string
     * Coordinates of the bounding box on a video frame where an Object is shown; in range [0..1].
     * The format is: `{x},{y},{width}x{height}`
     */
    RectAsString boundingBox = {0, 0, 0, 0};

    /**%apidoc[opt] */
    std::string imageUrl;

    /**%apidoc[opt] */
    std::vector<char> imageData;

    /**%apidoc[opt] */
    std::string imageDataFormat;

    /**%apidoc[opt] */
    std::vector<ApiObjectMetadataAttribute> attributes;

    bool operator==(const ApiBestShotMetadataPacket& other) const = default;
};
#define nx_vms_api_analytics_ApiBestShotMetadataPacket_Fields \
    (id)(deviceId)(trackId)(flags)(timestampMs)(boundingBox) \
    (imageUrl)(imageData)(imageDataFormat)(attributes)
QN_FUSION_DECLARE_FUNCTIONS(ApiBestShotMetadataPacket, (json), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(ApiBestShotMetadataPacket,
    nx_vms_api_analytics_ApiBestShotMetadataPacket_Fields)

/**%apidoc
 * Object Title metadata packet.
 */
struct NX_VMS_API ApiObjectTitleMetadataPacket
{
    /**%apidoc Engine id */
    nx::Uuid id;

    /**%apidoc
     * Device id (can be obtained from "id", "physicalId" or "logicalId" field via
     * `GET /rest/v{1-}/devices`) or MAC address (not supported for certain cameras).
     */
    QString deviceId;

    /**%apidoc[opt] */
    nx::Uuid trackId;

    /**%apidoc[opt] */
    Flags flags = Flags::none;

    /**%apidoc[opt] */
    std::chrono::milliseconds timestampMs;

    /**%apidoc[opt]:string
     * Coordinates of the bounding box on a video frame where an Object is shown; in range [0..1].
     * The format is: `{x},{y},{width}x{height}`
     */
    RectAsString boundingBox = {0, 0, 0, 0};

    /**%apidoc[opt] */
    std::string text;

    /**%apidoc[opt] */
    std::string imageUrl;

    /**%apidoc[opt] */
    std::vector<char> imageData;

    /**%apidoc[opt] */
    std::string imageDataFormat;

    bool operator==(const ApiObjectTitleMetadataPacket& other) const = default;
};
#define nx_vms_api_analytics_ApiObjectTitleMetadataPacket_Fields \
    (id)(deviceId)(trackId)(flags)(timestampMs)(boundingBox) \
    (text)(imageUrl)(imageData)(imageDataFormat)
QN_FUSION_DECLARE_FUNCTIONS(ApiObjectTitleMetadataPacket, (json), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(ApiObjectTitleMetadataPacket,
    nx_vms_api_analytics_ApiObjectTitleMetadataPacket_Fields)

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
QN_FUSION_DECLARE_FUNCTIONS(ApiEventMetadata, (json))
NX_REFLECTION_INSTRUMENT(ApiEventMetadata,
    nx_vms_api_analytics_ApiEventMetadata_Fields)

/**%apidoc
 * Event metadata packet.
 */
struct NX_VMS_API ApiEventMetadataPacket
{
    /**%apidoc Engine id */
    nx::Uuid id;

    /**%apidoc
     * Device id (can be obtained from "id", "physicalId" or "logicalId" field via
     * `GET /rest/v{1-}/devices`) or MAC address (not supported for certain cameras).
     */
    QString deviceId;

    /**%apidoc[opt] */
    Flags flags = Flags::none;

    /**%apidoc[opt] */
    std::chrono::milliseconds timestampMs;

    /**%apidoc[opt] */
    std::chrono::milliseconds durationMs;

    /**%apidoc[opt] */
    std::vector<ApiEventMetadata> events;

    bool operator==(const ApiEventMetadataPacket& other) const = default;
};
#define nx_vms_api_analytics_ApiEventMetadataPacket_Fields \
    (id)(deviceId)(flags)(timestampMs)(durationMs)(events)
QN_FUSION_DECLARE_FUNCTIONS(ApiEventMetadataPacket, (json), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(ApiEventMetadataPacket,
    nx_vms_api_analytics_ApiEventMetadataPacket_Fields)

struct NX_VMS_API RequestDeviceAgentManifest
{
    /**%apidoc
     * Flexible Device id. Can be obtained from "id", "physicalId" or "logicalId" field via
     * `GET /rest/v{1-}/devices` or MAC address (not supported for certain cameras).
     */
    std::string deviceId;

    /**%apidoc[opt]:uuidArray */
    json::ValueOrArray<nx::Uuid> engineId;
};
#define nx_vms_api_analytics_RequestDeviceAgentManifest_Fields (deviceId)(engineId)
QN_FUSION_DECLARE_FUNCTIONS(RequestDeviceAgentManifest, (json), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(RequestDeviceAgentManifest,
    nx_vms_api_analytics_RequestDeviceAgentManifest_Fields)

} // namespace nx::vms::api::analytics
