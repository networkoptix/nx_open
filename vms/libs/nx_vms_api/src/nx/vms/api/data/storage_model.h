// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <optional>
#include <tuple>
#include <type_traits>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/reflect/instrument.h>
#include <nx/utils/type_traits.h>

#include "media_server_data.h"
#include "resource_data.h"

namespace nx::vms::api {

/**%apidoc
 * Storage information object.
 * %param[opt]:object parameters Storage-specific key-value parameters.
 */
struct NX_VMS_API StorageModel: ResourceWithParameters
{
    nx::Uuid id;
    nx::Uuid serverId;

    /**%apidoc
     * %example Storage 1
     */
    QString name;
    QString path;

    /**%apidoc[opt]
     * Type of the method to access the Storage.
     *     %value "local"
     *     %value "network" Manually mounted NAS.
     *     %value "smb" Auto mounted NAS.
     */
    QString type;

    /**%apidoc:integer
     * Free space to maintain on the Storage, in bytes. Recommended value is 10 gigabytes for local
     * Storage, and 100 gigabytes for NAS.
     */
    std::optional<double> spaceLimitB;

    /**%apidoc[opt] Whether writing to the Storage is allowed. */
    bool isUsedForWriting = false;

    /**%apidoc[opt] Whether the Storage is used for backup. */
    bool isBackup = false;

    std::optional<ResourceStatus> status;

    using DbReadTypes = std::tuple<StorageData>;
    using DbListTypes = std::tuple<StorageDataList>;
    using DbUpdateTypes =
        std::tuple<StorageData, std::optional<ResourceStatusData>, ResourceParamWithRefDataList>;

    nx::Uuid getId() const { return id; }
    void setId(nx::Uuid id_) { id = std::move(id_); }
    static_assert(nx::utils::isCreateModelV<StorageModel>);
    static_assert(nx::utils::isUpdateModelV<StorageModel>);

    DbUpdateTypes toDbTypes() &&;
    static std::vector<StorageModel> fromDbTypes(DbListTypes data);
    static StorageModel fromDb(StorageData data);
};
#define StorageModel_Fields \
    (id)(serverId)(name)(path)(type)(spaceLimitB)(isUsedForWriting)(isBackup)(status)(parameters)

#define StorageModel_Funcs (csv_record)(json)(ubjson)(xml)
QN_FUSION_DECLARE_FUNCTIONS(StorageModel, StorageModel_Funcs, NX_VMS_API)
NX_REFLECTION_INSTRUMENT(StorageModel, StorageModel_Fields);

} // namespace nx::vms::api
