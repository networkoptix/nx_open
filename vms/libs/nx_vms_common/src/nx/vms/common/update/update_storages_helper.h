// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <optional>

#include <api/model/storage_status_reply.h>

namespace nx::vms::common::update::storage {

using ServerToStorages = std::pair<QnUuid, QnStorageSpaceDataList>;
using ServerToStoragesList = std::vector<ServerToStorages>;

NX_VMS_COMMON_API std::optional<nx::vms::api::StorageSpaceData> selectOne(
    const QnStorageSpaceDataList& candidates, int64_t minTotalSpaceGb);
NX_VMS_COMMON_API QList<QnUuid> selectServers(
    const ServerToStoragesList& serverToStorages, int64_t minStorageTotalSpaceGb);

} // namespace nx::vms::common::update::storage
