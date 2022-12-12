// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <map>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/reflect/enum_instrument.h>
#include <nx/utils/uuid.h>

#include "resource_data.h"
#include "storage_status_list.h"

namespace nx::vms::api {

struct NX_VMS_API StorageSpaceData
{
    QString url;
    QnUuid storageId;
    qint64 totalSpace = -1;
    qint64 freeSpace = -1;
    qint64 reservedSpace = 0;
    bool isExternal = false;
    bool isWritable = false;
    bool isUsedForWriting = false;
    bool isBackup = false;
    bool isOnline = false;
    QString storageType;
    StorageStatuses storageStatus;

    bool operator==(const StorageSpaceData& other) const = default;
};
#define StorageSpaceData_Fields \
    (url)(storageId)(totalSpace)(freeSpace)(reservedSpace)(isExternal)(isWritable) \
    (isUsedForWriting)(storageType)(isBackup)(isOnline)(storageStatus)
QN_FUSION_DECLARE_FUNCTIONS(StorageSpaceData, (csv_record)(json)(ubjson)(xml), NX_VMS_API)

struct NX_VMS_API StorageSpaceDataWithDbInfo: StorageSpaceData
{
    QnUuid serverId;
    QString name;

    StorageSpaceDataWithDbInfo() = default;
    StorageSpaceDataWithDbInfo(StorageSpaceData&& base): StorageSpaceData(base) {}

    bool operator==(const StorageSpaceDataWithDbInfo& other) const = default;
};
#define StorageSpaceDataWithDbInfo_Fields StorageSpaceData_Fields (serverId)(name)
QN_FUSION_DECLARE_FUNCTIONS(
    StorageSpaceDataWithDbInfo, (csv_record)(json)(ubjson)(xml), NX_VMS_API)

using StorageSpaceDataWithDbInfoList = std::vector<StorageSpaceDataWithDbInfo>;

} // namespace nx::vms::api
