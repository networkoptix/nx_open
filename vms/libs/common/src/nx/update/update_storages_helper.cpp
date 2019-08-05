#include "update_storages_helper.h"

namespace nx::update::storage {

std::optional<QnStorageSpaceData> selectOne(const QnStorageSpaceDataList& candidates)
{
    QnStorageSpaceDataList filtered;
    std::copy_if(
        candidates.cbegin(), candidates.cend(), std::back_inserter(filtered),
        [](const QnStorageSpaceData& data)
        {
            static const int64_t kMinUpdatePersistentStorageTotalSpace = 1024 * 1024 * 1024LL; // 1 GB
            return
                data.isWritable
                && data.freeSpace > data.reservedSpace * 0.9
                && data.totalSpace > kMinUpdatePersistentStorageTotalSpace;
        });

    std::sort(
        filtered.begin(), filtered.end(),
        [](const auto& data1, const auto& data2) { return data1.totalSpace > data2.totalSpace; });

    return filtered.empty() ? std::nullopt : std::optional<QnStorageSpaceData>(filtered[0]);
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

}