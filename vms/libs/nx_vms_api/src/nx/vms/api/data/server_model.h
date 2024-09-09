// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <optional>
#include <tuple>
#include <type_traits>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/reflect/instrument.h>

#include "media_server_data.h"
#include "module_information.h" //< For nx::utils::OsInfo fusion.
#include "port_forwarding_configuration.h"
#include "storage_model.h"

namespace nx::vms::api {

/**%apidoc Server information object.
 * %param[opt]:object parameters Server-specific key-value parameters.
 * %param[opt]:array backupBitrateBytesPerSecond
 *     Backup bitrate per day of week and hour, as a JSON array of key-value objects, structured
 *     according to the following example:
 *     <pre><code>
 *     [
 *         {
 *             "key": { "day": "DAY_OF_WEEK", "hour": HOUR },
 *             "value": BYTES_PER_SECOND
 *         },
 *         ...
 *     ]
 *     </code></pre>
 *     Here <code>DAY_OF_WEEK</code> is one of <code>monday</code>, <code>tuesday</code>,
 *     <code>wednesday</code>, <code>thursday</code>, <code>friday</code>, <code>saturday</code>,
 *     <code>sunday</code>; <code>HOUR</code> is an integer in range 0..23;
 *     <code>BYTES_PER_SECOND</code> is an integer that can be represented as a number or a string.
 *     <br/>
 *     For any day-hour position, a missing value means "unlimited bitrate", and a zero value means
 *     "don't perform backup".
 * %param:object backupBitrateBytesPerSecond[].key
 * %param:enum backupBitrateBytesPerSecond[].key.day
 *     %value monday
 *     %value tuesday
 *     %value wednesday
 *     %value thursday
 *     %value friday
 *     %value saturday
 *     %value sunday
 * %param:integer backupBitrateBytesPerSecond[].key.hour
 *     %example 0
 * %param:string backupBitrateBytesPerSecond[].value
 *     %example 0
 */
struct NX_VMS_API ServerModelBase: ResourceWithParameters
{
    nx::Uuid id;

    /**%apidoc
     * %example Server 1
     */
    QString name;

    /**%apidoc
     * %example https://127.0.0.1:7001
     */
    QString url;

    /**%apidoc[readonly] */
    QString version;

    /**%apidoc[opt] */
    QString authKey;

    /**%apidoc[readonly] */
    std::optional<nx::utils::OsInfo> osInfo;

    /**%apidoc[readonly] */
    ServerFlags flags = SF_None;

    BackupBitrateBytesPerSecond backupBitrateBytesPerSecond;

    /**%apidoc[readonly] */
    ResourceStatus status = ResourceStatus::undefined;

    /**%apidoc[readonly] */
    std::vector<StorageModel> storages;

    /**%apidoc[readonly] */
    std::vector<PortForwardingConfiguration> portForwardingConfigurations;

    using DbReadTypes = std::tuple<
        MediaServerData,
        MediaServerUserAttributesData,
        ResourceStatusDataList,
        ResourceParamWithRefDataList,
        StorageDataList>;

    using DbUpdateTypes = std::tuple<
        MediaServerData,
        std::optional<MediaServerUserAttributesData>,
        ResourceParamWithRefDataList>;

    using DbListTypes = std::tuple<
        MediaServerDataList,
        MediaServerUserAttributesDataList,
        ResourceStatusDataList,
        ResourceParamWithRefDataList,
        StorageDataList>;

};
#define ServerModelBase_Fields \
    ResourceWithParameters_Fields \
    (id)(name)(url)(version)(authKey)(osInfo)(flags) \
    (backupBitrateBytesPerSecond) \
    (status)(storages)(portForwardingConfigurations)
QN_FUSION_DECLARE_FUNCTIONS(ServerModelBase, (json), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(ServerModelBase, ServerModelBase_Fields);

struct NX_VMS_API ServerModelV1: ServerModelBase
{
    /**%apidoc[opt] */
    std::vector<QString> endpoints;

    /**%apidoc[opt]
     * If `isFailoverEnabled` is true then Devices are moved across the Servers with the same `locationId`.
     */
    int locationId = 0;

    /**%apidoc[opt] */
    bool isFailoverEnabled = false;

    /**%apidoc[opt] */
    int maxCameras = 0;

    DbUpdateTypes toDbTypes() &&;
    static std::vector<ServerModelV1> fromDbTypes(DbListTypes data);
};
#define ServerModelV1_Fields \
    ServerModelBase_Fields \
    (endpoints)(isFailoverEnabled)(locationId)(maxCameras)
QN_FUSION_DECLARE_FUNCTIONS(ServerModelV1, (json), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(ServerModelV1, ServerModelV1_Fields);

struct NX_VMS_API ServerNetwork
{
    /**%apidoc[opt] */
    std::vector<QString> endpoints;

    /**%apidoc[opt]
     * Server-generated self-signed certificate in PEM format without private key. It is used by
     * the Server if Server-specific SNI is required by a Client or there is no the user-provided
     * certificate or the user-provided certificate is invalid.
     */
    std::string certificatePem;

    /**%apidoc[readonly]
     * The content of a user-provided file `cert.pem` on the Server file system in PEM format
     * without private key. Empty if not provided or can't be loaded and parsed. It is used by
     * the Server if no Server-specific SNI is required by a Client.
     */
    std::string userProvidedCertificatePem;

    /**%apidoc[readonly] */
    QString publicIp;

    /**%apidoc[readonly] */
    std::map<QString, QString> networkInterfaces;
};
#define ServerNetwork_Fields \
    (endpoints)(certificatePem)(userProvidedCertificatePem) \
    (publicIp)(networkInterfaces)
QN_FUSION_DECLARE_FUNCTIONS(ServerNetwork, (json), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(ServerNetwork, ServerNetwork_Fields);

struct NX_VMS_API ServerSettings
{
    /**%apidoc[opt]
     * If `isFailoverEnabled` is true then Devices are moved across the Servers with the same `locationId`.
     */
    int locationId = 0;

    /**%apidoc[opt] */
    bool isFailoverEnabled = false;

    /**%apidoc[opt] */
    int maxCameras = 0;

    /**%apidoc[opt] */
    bool webCamerasDiscoveryEnabled = false;
};
#define ServerSettings_Fields \
    (isFailoverEnabled)(locationId)(maxCameras)(webCamerasDiscoveryEnabled)
QN_FUSION_DECLARE_FUNCTIONS(ServerSettings, (json), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(ServerSettings, ServerSettings_Fields);

struct NX_VMS_API ServerHardwareInformation
{
    /**%apidoc[readonly]:integer */
    double physicalMemoryB = 0;

    /**%apidoc[readonly] */
    QString cpuArchitecture;

    /**%apidoc[readonly] */
    QString cpuModelName;

    bool operator==(const ServerHardwareInformation& other) const = default;
};
#define ServerHardwareInformation_Fields \
    (physicalMemoryB)(cpuArchitecture)(cpuModelName)
QN_FUSION_DECLARE_FUNCTIONS(ServerHardwareInformation, (json), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(ServerHardwareInformation, ServerHardwareInformation_Fields);

struct ServerRuntimeInfo
{
    /**%apidoc[readonly] */
    std::optional<ServerHardwareInformation> hardwareInformation;

    /**%apidoc[readonly] */
    std::optional<ServerTimeZoneInformation> timezone;

    /**%apidoc[readonly] */
    bool idConflictDetected = false;
};
#define ServerRuntimeInfo_Fields \
    (hardwareInformation)(timezone)(idConflictDetected)
QN_FUSION_DECLARE_FUNCTIONS(ServerRuntimeInfo, (json), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(ServerRuntimeInfo, ServerRuntimeInfo_Fields);

struct NX_VMS_API ServerModelV4: ServerModelBase
{
    std::optional<ServerNetwork> network;

    std::optional<ServerSettings> settings;

    /**%apidoc[readonly] */
    std::optional<ServerRuntimeInfo> runtimeInformation;

    DbUpdateTypes toDbTypes() &&;
    static std::vector<ServerModelV4> fromDbTypes(DbListTypes data);
};
#define ServerModelV4_Fields \
    ServerModelBase_Fields \
    (network)(settings)(runtimeInformation)
NX_VMS_API_DECLARE_STRUCT_AND_LIST_EX(ServerModelV4, (json))
NX_REFLECTION_INSTRUMENT(ServerModelV4, ServerModelV4_Fields);

using ServerModel = ServerModelV4;

} // namespace nx::vms::api
