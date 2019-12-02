#include "writable_storages_helper.h"
#include "storage_manager.h"
#include <nx/utils/random.h>

namespace nx::vms::server {

WritableStoragesHelper::SpaceInfo WritableStoragesHelper::SpaceInfo::from(
    const StorageResourcePtr& storage,
    const QnStorageManager* owner)
{
    SpaceInfo result;
    result.url = nx::utils::url::hidePassword(storage->getUrl()).toStdString();
    result.totalSpace = getOrThrow(storage->getTotalSpace(), "total space", result.url);
    result.freeSpace = getOrThrow(storage->getFreeSpace(), "free space", result.url);
    result.spaceLimit = getOrThrow(storage->getSpaceLimit(), "space limit", result.url);
    if (owner)
    {
        result.nxOccupied = getOrThrow(
            owner->occupiedSpace(owner->storageDbPool()->getStorageIndex(storage)),
            "nx occupied space", result.url);
    }
    else
    {
        result.nxOccupied = 0LL;
    }

    result.available = result.totalSpace - result.spaceLimit;
    result.effective = result.nxOccupied + result.freeSpace - result.spaceLimit;
    result.isSystem = storage->isSystem();
    result.storage = storage;

    return result;
}

nx::vms::server::StorageResourceList WritableStoragesHelper::SpaceInfo::toStorageList(
    const std::vector<SpaceInfo>& infos)
{
    nx::vms::server::StorageResourceList result;
    std::transform(
        infos.cbegin(), infos.cend(), std::back_inserter(result),
        [](const auto& info) { return info.storage; });
    return result;
}

std::string WritableStoragesHelper::SpaceInfo::toString() const
{
    std::stringstream ss;
    ss
        << "SpaceInfo { totalSpace: " << totalSpace << ", "
        << "freeSpace: " << freeSpace << ", "
        << "spaceLimit: " << spaceLimit << ", "
        << "nxOccupied: " << nxOccupied << ", "
        << "available: " << available << ", "
        << "effective: " << effective << ", "
        << "url: " << url << ", "
        << "isSystem: " << isSystem << " }";

    return ss.str();
}

int64_t WritableStoragesHelper::SpaceInfo::getOrThrow(
    int64_t value,
    const std::string& description,
    const std::string& url)
{
    if (value < 0)
    {
        std::stringstream ss;
        ss << "Failed to get " << description << " for the storage " << url;
        throw std::runtime_error(ss.str());
    }

    return value;
}

WritableStoragesHelper::WritableStoragesHelper(const QnStorageManager* owner): m_owner(owner)
{
}

StorageResourceList WritableStoragesHelper::list(const StorageResourceList& storages) const
{
    auto candidates = online(toInfos(storages, m_owner));
    const auto resultStorages = SpaceInfo::toStorageList(resultInfos(candidates));
    adjustFlags(SpaceInfo::toStorageList(candidates), resultStorages);
    return resultStorages;
}

std::vector<WritableStoragesHelper::SpaceInfo> WritableStoragesHelper::toInfos(
    const StorageResourceList& storages, const QnStorageManager* owner)
{
    std::vector<SpaceInfo> result;
    const auto candidates = unique(storages);
    for (const auto& storage: candidates)
    {
        try
        {
            result.push_back(SpaceInfo::from(storage, owner));
        }
        catch(const std::exception& e)
        {
            NX_DEBUG(typeid(WritableStoragesHelper), e.what());
            continue;
        }
    }

    return result;
}

StorageResourceList WritableStoragesHelper::filterOutSmall(const StorageResourceList& storages)
{
    return SpaceInfo::toStorageList(filterOutSmallSystem(filterOutSmall(toInfos(storages, nullptr))));
}

StorageResourcePtr WritableStoragesHelper::optimalStorageForRecording(
    const StorageResourceList& storages) const
{
    int64_t totalEffectiveSpace = 0LL;
    const auto infos = filterOutUnused(resultInfos(online(toInfos(storages, m_owner))));
    NX_DEBUG(this, "Optimal storage selection: candidates number: %1", infos.size());
    for (const auto& info: infos)
    {
        NX_DEBUG(this, "Optimal storage selection: candidate: %1", info);
        totalEffectiveSpace += info.effective;
    }

    const double selectionPoint = nx::utils::random::number<double>(0, 1);
    NX_DEBUG(
        this, "Optimal storage selection: total effective space: %1, selection point: %2",
        totalEffectiveSpace, selectionPoint);

    double runningSum = 0.0;
    for (const auto& info: infos)
    {
        runningSum += static_cast<double>(info.effective) / totalEffectiveSpace;
        NX_DEBUG(this, "Optimal storage selection: running sum: %1", runningSum);
        if (selectionPoint < runningSum)
        {
            NX_DEBUG(this, "Optimal storage selection: final result: %1", info.url);
            return info.storage;
        }
    }

    return StorageResourcePtr();
}

std::vector<WritableStoragesHelper::SpaceInfo> WritableStoragesHelper::resultInfos(
    const std::vector<SpaceInfo>& candidates) const
{
    auto result = filterOutSpaceless(candidates);
    result = filterOutSmall(result);
    return filterOutSmallSystem(result);
}

std::vector<WritableStoragesHelper::SpaceInfo> WritableStoragesHelper::online(
    const std::vector<SpaceInfo>& infos)
{
    std::vector<SpaceInfo> result;
    std::copy_if(
        infos.cbegin(), infos.cend(), std::back_inserter(result),
        [](const auto& info) { return info.storage->isOnline(); });
    return result;
}

StorageResourceList WritableStoragesHelper::unique(const StorageResourceList& candidates)
{
    auto result = candidates;
    std::sort(
        result.begin(), result.end(),
        [](const auto& storage1, const auto& storage2)
        {
            return storage1->getUrl() < storage2->getUrl();
        });

    auto last = std::unique(
        result.begin(), result.end(),
        [](const auto& storage1, const auto& storage2)
        {
            return storage1->getUrl() == storage2->getUrl();
        });

    result.erase(last, result.end());
    NX_ASSERT(candidates.size() == result.size());
    if (candidates.size() != result.size())
    {
        QString diffString;
        for (const auto& storage: candidates)
        {
            if (!result.contains(storage))
            {
                diffString +=
                    QString("(%1) ").arg(nx::utils::url::hidePassword(storage->getUrl()));
            }
        }
        NX_WARNING(typeid(WritableStoragesHelper), "Found duplicate storages: %1", diffString);
    }

    return result;
}

std::vector<WritableStoragesHelper::SpaceInfo> WritableStoragesHelper::filterOutSpaceless(
    const std::vector<SpaceInfo>& infos)
{
    std::vector<SpaceInfo> result;
    for (const auto& info: infos)
    {
        if (info.effective < 0)
        {
            NX_DEBUG(
                typeid(WritableStoragesHelper),
                "Skipping storage (%1) because nxOccupiedSpace (%2) + freeSpace (%3) - spaceLimit (%4) < 0",
                info.url, info.nxOccupied, info.freeSpace, info.spaceLimit);
            continue;
        }
        result.push_back(info);
    }

    return result;
}

std::vector<WritableStoragesHelper::SpaceInfo> WritableStoragesHelper::filterOutUnused(
    const std::vector<SpaceInfo>& infos)
{
    std::vector<SpaceInfo> result;
    std::copy_if(
        infos.cbegin(), infos.cend(), std::back_inserter(result),
        [](const auto& info) { return info.storage->isUsedForWriting(); });
    return result;
}

std::vector<WritableStoragesHelper::SpaceInfo> WritableStoragesHelper::filterOutSmall(
    const std::vector<SpaceInfo>& infos)
{
    const auto maxIt = std::max_element(
        infos.cbegin(), infos.cend(),
        [](const auto& info1, const auto& info2) { return info1.available < info2.available; });

    if (maxIt == infos.cend())
        return infos;

    const auto maxSpaceTreshold = maxIt->available / QnStorageManager::kBigStorageTreshold;
    std::vector<SpaceInfo> result;
    std::copy_if(
        infos.cbegin(), infos.cend(), std::back_inserter(result),
        [maxSpaceTreshold](const auto& info) { return info.available >= maxSpaceTreshold; });

    return result;
}

std::vector<WritableStoragesHelper::SpaceInfo> WritableStoragesHelper::filterOutSmallSystem(
    const std::vector<WritableStoragesHelper::SpaceInfo>& infos)
{
    int64_t totalNonSystemSpaceAvailable = 0;
    for (const auto& info: infos)
    {
        if (!info.isSystem)
            totalNonSystemSpaceAvailable += info.available;
    }

    std::vector<SpaceInfo> result;
    std::copy_if(
        infos.cbegin(), infos.cend(), std::back_inserter(result),
        [totalNonSystemSpaceAvailable](const auto& info)
        {
            if (!info.isSystem)
                return true;

            return totalNonSystemSpaceAvailable <= info.available * 5;
        });

    return result;
}

void WritableStoragesHelper::adjustFlags(
    const StorageResourceList& candidates, const StorageResourceList& result)
{
    for (const auto& storage: candidates)
    {
        if (result.contains(storage))
            storage->setStatusFlag(storage->statusFlag() & ~Qn::StorageStatus::tooSmall);
        else
            storage->setStatusFlag(storage->statusFlag() | Qn::StorageStatus::tooSmall);
    }
}

} // namespace nx::vms::server
