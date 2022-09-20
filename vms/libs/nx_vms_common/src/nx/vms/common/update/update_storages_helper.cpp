// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "update_storages_helper.h"

namespace nx::vms::common::update::storage {

std::optional<nx::vms::api::StorageSpaceData> selectOne(const QnStorageSpaceDataList& candidates)
{
    QnStorageSpaceDataList filtered;
    std::copy_if(
        candidates.cbegin(), candidates.cend(), std::back_inserter(filtered),
        [](const nx::vms::api::StorageSpaceData& data)
        {
            static constexpr int64_t kMinUpdatePersistentStorageTotalSpace = 1024LL * 1024 * 1024 * 1024; // 1 TB
            return
                data.isWritable
                && data.freeSpace > data.reservedSpace * 0.9
                && data.totalSpace > kMinUpdatePersistentStorageTotalSpace
                && !data.isExternal;
        });

    std::sort(
        filtered.begin(), filtered.end(),
        [](const auto& data1, const auto& data2) { return data1.totalSpace > data2.totalSpace; });

    return filtered.empty()
        ? std::nullopt
        : std::optional<nx::vms::api::StorageSpaceData>(filtered[0]);
}

QList<QnUuid> selectServers(const ServerToStoragesList& serverToStorages)
{
    std::vector<std::pair<QnUuid, int64_t>> serverToMaxStorageSpace;
    for (const auto& p: serverToStorages)
    {
        const auto bestStorage = selectOne(p.second);
        if (!bestStorage)
            continue;

        serverToMaxStorageSpace.push_back(std::make_pair(p.first, bestStorage->totalSpace));
    }

    if (serverToMaxStorageSpace.empty())
        return QList<QnUuid>();

    std::sort(
        serverToMaxStorageSpace.begin(), serverToMaxStorageSpace.end(),
        [](const auto& p1, const auto& p2) { return p1.second > p2.second; });

    QList<QnUuid> result;
    for (size_t i = 0; i < std::max<size_t>(1, serverToMaxStorageSpace.size() / 10); ++i)
        result.push_back(serverToMaxStorageSpace[i].first);

    return result;
}

} // namespace nx::vms::common::update::storage
