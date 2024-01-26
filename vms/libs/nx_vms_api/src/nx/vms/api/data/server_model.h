// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <optional>
#include <tuple>
#include <type_traits>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/reflect/instrument.h>

#include "media_server_data.h"
#include "module_information.h" //< For nx::utils::OsInfo fusion.
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
struct NX_VMS_API ServerModel: ResourceWithParameters
{
    QnUuid id;

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
    std::vector<QString> endpoints;

    /**%apidoc[opt] */
    QString authKey;

    /**%apidoc[readonly] */
    std::optional<nx::utils::OsInfo> osInfo;

    /**%apidoc[readonly] */
    ServerFlags flags = SF_None;

    /**%apidoc[opt]
     * If `isFailoverEnabled` is true then Devices are moved across the Servers with the same `locationId`.
     */
    int locationId = 0;

    /**%apidoc[opt] */
    bool isFailoverEnabled = false;

    /**%apidoc[opt] */
    int maxCameras = 0;

    BackupBitrateBytesPerSecond backupBitrateBytesPerSecond;

    /**%apidoc[readonly] */
    ResourceStatus status = ResourceStatus::undefined;

    /**%apidoc[readonly] */
    std::vector<StorageModel> storages;

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

    QnUuid getId() const { return id; }
    void setId(QnUuid id_) { id = std::move(id_); }
    static_assert(isCreateModelV<ServerModel>);
    static_assert(isUpdateModelV<ServerModel>);
    static_assert(isFlexibleIdModelV<ServerModel>);

    DbUpdateTypes toDbTypes() &&;
    static std::vector<ServerModel> fromDbTypes(DbListTypes data);
};
#define ServerModel_Fields \
    (id)(name)(url)(version)(endpoints)(authKey)(osInfo)(flags) \
    (isFailoverEnabled)(locationId)(maxCameras)(backupBitrateBytesPerSecond) \
    (status)(storages)(parameters)
QN_FUSION_DECLARE_FUNCTIONS(ServerModel, (json), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(ServerModel, ServerModel_Fields);

} // namespace nx::vms::api
