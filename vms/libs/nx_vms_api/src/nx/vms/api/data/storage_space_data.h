// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/fusion/model_functions_fwd.h>
#include <nx/reflect/instrument.h>
#include <nx/utils/serialization/flags.h>
#include <nx/utils/uuid.h>
#include <nx/vms/api/data/storage_flags.h>

namespace nx::vms::api {

// Deprecated
NX_REFLECTION_ENUM(StorageStatus,
    none = 0,
    used = 1 << 1,
    tooSmall = 1 << 2,
    system = 1 << 3,
    removable = 1 << 4,
    beingChecked = 1 << 5,
    beingRebuilt = 1 << 6,
    disabled = 1 << 7,
    dbReady = 1 << 8
)

Q_DECLARE_FLAGS(StorageStatuses, StorageStatus)
Q_DECLARE_OPERATORS_FOR_FLAGS(StorageStatuses)

struct NX_VMS_API StorageSpaceDataBase
{
    QString url;
    nx::Uuid storageId;
    qint64 totalSpace = -1;
    qint64 freeSpace = -1;
    qint64 reservedSpace = 0;
    bool isExternal = false;
    bool isWritable = false;
    bool isUsedForWriting = false;
    bool isBackup = false;
    bool isOnline = false;
    QString storageType;

    bool operator==(const StorageSpaceDataBase&) const = default;
};
#define StorageSpaceDataBase_Fields \
    (url)(storageId)(totalSpace)(freeSpace)(reservedSpace)(isExternal)(isWritable) \
    (isUsedForWriting)(storageType)(isBackup)(isOnline)
NX_REFLECTION_INSTRUMENT(StorageSpaceDataBase, StorageSpaceDataBase_Fields)
QN_FUSION_DECLARE_FUNCTIONS(StorageSpaceDataBase, (json)(ubjson), NX_VMS_API)

struct NX_VMS_API StorageSpaceDataV1: StorageSpaceDataBase
{
    StorageStatuses storageStatus;

    StorageSpaceDataV1(const StorageSpaceDataBase& base): StorageSpaceDataBase(base) {}
    StorageSpaceDataV1() = default;
    bool operator==(const StorageSpaceDataV1&) const = default;
};
#define StorageSpaceDataV1_Fields StorageSpaceDataBase_Fields (storageStatus)
NX_REFLECTION_INSTRUMENT(StorageSpaceDataV1, StorageSpaceDataV1_Fields)
QN_FUSION_DECLARE_FUNCTIONS(StorageSpaceDataV1, (json)(ubjson), NX_VMS_API)

using StorageSpaceDataListV1 = std::vector<StorageSpaceDataV1>;

struct NX_VMS_API StorageSpaceDataV3: StorageSpaceDataBase
{
    nx::vms::api::StorageRuntimeFlags runtimeFlags;
    nx::vms::api::StoragePersistentFlags persistentFlags;

    StorageSpaceDataV3(const StorageSpaceDataBase& base): StorageSpaceDataBase(base) {}
    StorageSpaceDataV3() = default;
    static StorageSpaceDataListV1 toV1List(const std::vector<StorageSpaceDataV3>& v3List);
    StorageSpaceDataV1 toV1() const;
};
#define StorageSpaceDataV3_Fields  StorageSpaceDataBase_Fields (runtimeFlags)(persistentFlags)
NX_REFLECTION_INSTRUMENT(StorageSpaceDataV3, StorageSpaceDataV3_Fields)
QN_FUSION_DECLARE_FUNCTIONS(StorageSpaceDataV3, (json)(ubjson), NX_VMS_API)

using StorageSpaceData = StorageSpaceDataV3;
using StorageSpaceDataListV3 = std::vector<StorageSpaceDataV3>;
using StorageSpaceDataList = StorageSpaceDataListV3;

struct NX_VMS_API StorageSpaceDataWithDbInfoV1: StorageSpaceDataV1
{
    nx::Uuid serverId;
    QString name;

    StorageSpaceDataWithDbInfoV1(StorageSpaceDataV1&& base): StorageSpaceDataV1(base) {}
    StorageSpaceDataWithDbInfoV1() = default;
    bool operator==(const StorageSpaceDataWithDbInfoV1&) const = default;
};

#define StorageSpaceDataWithDbInfoV1_Fields StorageSpaceDataV1_Fields (serverId)(name)
NX_REFLECTION_INSTRUMENT(StorageSpaceDataWithDbInfoV1, StorageSpaceDataWithDbInfoV1_Fields)
QN_FUSION_DECLARE_FUNCTIONS(StorageSpaceDataWithDbInfoV1, (json)(ubjson), NX_VMS_API)

using StorageSpaceDataWithDbInfoListV1 = std::vector<StorageSpaceDataWithDbInfoV1>;

struct NX_VMS_API StorageSpaceDataWithDbInfoV3: StorageSpaceDataV3
{
    nx::Uuid serverId;
    QString name;

    StorageSpaceDataWithDbInfoV3(StorageSpaceDataV3&& base): StorageSpaceDataV3(base) {}
    StorageSpaceDataWithDbInfoV3() = default;

    StorageSpaceDataWithDbInfoV1 toV1() const;
    static StorageSpaceDataWithDbInfoListV1 toV1List(
        const std::vector<StorageSpaceDataWithDbInfoV3>& v3List);
};

#define StorageSpaceDataWithDbInfoV3_Fields StorageSpaceDataV3_Fields (serverId)(name)
NX_REFLECTION_INSTRUMENT(StorageSpaceDataWithDbInfoV3, StorageSpaceDataWithDbInfoV3_Fields)
QN_FUSION_DECLARE_FUNCTIONS(StorageSpaceDataWithDbInfoV3, (json)(ubjson), NX_VMS_API)

using StorageSpaceDataWithDbInfo = StorageSpaceDataWithDbInfoV3;
using StorageSpaceDataWithDbInfoListV3 = std::vector<StorageSpaceDataWithDbInfoV3>;
using StorageSpaceDataWithDbInfoList = StorageSpaceDataWithDbInfoListV3;

constexpr const char* const kCloudStorageType = "cloud";

} // namespace nx::vms::api
