// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <optional>

#include <nx/vms/api/data/storage_space_data.h>

namespace nx::vms::common::update::storage {

using ServerToStorages = std::pair<nx::Uuid, nx::vms::api::StorageSpaceDataList>;
using ServerToStoragesList = std::vector<ServerToStorages>;

NX_VMS_COMMON_API std::optional<nx::vms::api::StorageSpaceData> selectOne(
    const nx::vms::api::StorageSpaceDataList& candidates, int64_t minTotalSpaceGb);

NX_VMS_COMMON_API QList<nx::Uuid> selectServers(
    const ServerToStoragesList& serverToStorages, int64_t minStorageTotalSpaceGb);

} // namespace nx::vms::common::update::storage
