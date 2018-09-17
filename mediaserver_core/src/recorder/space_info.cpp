#include <algorithm>
#include <iterator>
#include <nx/utils/log/log.h>
#include <nx/utils/literal.h>
#include "space_info.h"

namespace nx{
namespace recorder {

namespace {

static bool hasStorageWithoutEffectiveSpace(const SpaceInfo::SpaceInfoVector& infos)
{
    return std::any_of(
        infos.cbegin(),
        infos.cend(),
        [](const SpaceInfo::StorageSpaceInfo& info)
        {
            return info.isEffectiveSpaceSet();
        });
}

} // namespace

SpaceInfo::SpaceInfo() : m_gen(m_rd()) {}

SpaceInfo::SpaceInfoVector::iterator SpaceInfo::storageByIndex(int index)
{
    return std::find_if(m_storageSpaceInfo.begin(), m_storageSpaceInfo.end(),
                        [index](const StorageSpaceInfo& info) { return info.index == index; });
}

void SpaceInfo::storageAdded(int index, int64_t totalSpace)
{
    QnMutexLocker lock(&m_mutex);
    auto storageIndexIt = storageByIndex(index);
    bool storageIndexNotFound = storageIndexIt == m_storageSpaceInfo.cend();

    if (!storageIndexNotFound)
    {
        NX_DEBUG(this, lit("[Storage, SpaceInfo, Selection] This storage index (%1) already added")
                .arg(index));
        storageIndexIt->totalSpace = totalSpace;
        return;
    }

    NX_VERBOSE(this, lit("[Storage, SpaceInfo, Selection] Adding storage index (%1)")
            .arg(index));
    m_storageSpaceInfo.emplace_back(index, totalSpace);
}

void SpaceInfo::storageChanged(int index, int64_t freeSpace, int64_t nxOccupiedSpace, int64_t spaceLimit)
{
    QnMutexLocker lock(&m_mutex);
    auto storageIndexIt = storageByIndex(index);
    NX_ASSERT(storageIndexIt != m_storageSpaceInfo.cend());
    if (storageIndexIt == m_storageSpaceInfo.cend())
        return;

    storageIndexIt->effectiveSpace = std::max<int64_t>(freeSpace + nxOccupiedSpace - spaceLimit, 0LL);
    NX_VERBOSE(this, lit("[Storage, SpaceInfo, Selection] Calculating effective space for storage %1. \
Free space = %2, nxOccupiedSpace = %3, spaceLimit = %4, effectiveSpace = %5")
        .arg(storageIndexIt->index)
        .arg(freeSpace)
        .arg(nxOccupiedSpace)
        .arg(spaceLimit)
        .arg(storageIndexIt->effectiveSpace));
}

void SpaceInfo::storageRemoved(int index)
{
    QnMutexLocker lock(&m_mutex);
    auto toRemoveIt = std::find_if(m_storageSpaceInfo.begin(), m_storageSpaceInfo.end(),
        [index](const StorageSpaceInfo& info)
        {
            return info.index == index;
        });

    if (toRemoveIt == m_storageSpaceInfo.cend())
    {
        NX_DEBUG(this, lit("[Storage, SpaceInfo, Selection] Storage index %1 removed, but had not been added")
                .arg(index));
        return;
    }
    NX_VERBOSE(this, lit("[Storage, SpaceInfo, Selection] Removing storage index (%1)")
            .arg(index));
    m_storageSpaceInfo.erase(toRemoveIt);
}

int SpaceInfo::getOptimalStorageIndex(const std::vector<int>& allowedIndexes) const
{
    QnMutexLocker lock(&m_mutex);

    /* get filtered by predicate storages vector */
    SpaceInfoVector filteredSpaceInfo;
    for (const auto& spaceInfo: m_storageSpaceInfo)
    {
        if (std::find_if(
                allowedIndexes.cbegin(),
                allowedIndexes.cend(),
                [&spaceInfo](int index) { return index == spaceInfo.index; }) != allowedIndexes.cend())
        {
            filteredSpaceInfo.emplace_back(spaceInfo);
        }
    }

    if(filteredSpaceInfo.empty())
    {
        NX_DEBUG(this, lit("[Storage, SpaceInfo, Selection] Failed to find approprirate storage index"));
        return -1;
    }

    /* use totalSpace based algorithm if effective space is unknown for any of the storages */
    if (hasStorageWithoutEffectiveSpace(filteredSpaceInfo))
        return getStorageIndexImpl(filteredSpaceInfo, false);

    return getStorageIndexImpl(filteredSpaceInfo, true);
}

int SpaceInfo::getStorageIndexImpl(const SpaceInfoVector& filteredSpaceInfo, bool byEffectiveSpace) const
{
    /* select storage index based on their effective (or total) spaces.
    *  Space plays a 'weight' role, but the selection algorithm is essentially random
    */
    double randomSelectionPoint = 0.5;
    try
    {
        randomSelectionPoint = std::uniform_real_distribution<>(0, 1)(m_gen);
    }
    catch (const std::exception&)
    {
        NX_DEBUG(this, lit("[Storage, SpaceInfo, Selection] Exception while selecting random point"));
    }

    int64_t totalEffectiveSpace = 0;
    for (const auto& spaceInfo: filteredSpaceInfo)
        totalEffectiveSpace += (byEffectiveSpace ? spaceInfo.effectiveSpace : spaceInfo.totalSpace);

    if (totalEffectiveSpace == 0)
    {
        NX_DEBUG(this, "[Storage, SpaceInfo, Selection] No appropriate candidate found");
        return -1;
    }

    NX_VERBOSE(this, lit("[Storage, SpaceInfo, Selection] Calculating optimal storage index. \
Candidates count = %1, byEffectiveSpace = %2, totalSpace = %3, selection point = %4")
        .arg(filteredSpaceInfo.size())
        .arg(byEffectiveSpace)
        .arg(totalEffectiveSpace)
        .arg(randomSelectionPoint));

    long double proportionSum = 0.0;
    for (const auto& spaceInfo: filteredSpaceInfo)
    {
        auto spaceToUse = byEffectiveSpace ? spaceInfo.effectiveSpace : spaceInfo.totalSpace;
        long double storageProportion = (long double)spaceToUse / totalEffectiveSpace;
        proportionSum += storageProportion;

        NX_VERBOSE(this, lit("[Storage, SpaceInfo, Selection] Storage proportion for storage %1 = %2")
            .arg(spaceInfo.index)
            .arg((double)storageProportion));

        if (proportionSum > randomSelectionPoint)
        {
            NX_VERBOSE(this, lit("[Storage, SpaceInfo, Selection] Selected storage index %1")
                .arg(spaceInfo.index));
            return spaceInfo.index;
        }
    }

    NX_ASSERT(false);
    NX_DEBUG(this, lit("[Storage, SpaceInfo, Selection] No storage index found."));

    return -1;
}

SpaceInfo::RecordingReadinessState SpaceInfo::state(int storageIndex) const
{
    QnMutexLocker lock(&m_mutex);
    if (hasStorageWithoutEffectiveSpace(m_storageSpaceInfo))
        return enoughSpace;

    auto it = std::find_if(
                m_storageSpaceInfo.cbegin(),
                m_storageSpaceInfo.cend(),
                [storageIndex](const StorageSpaceInfo& info)
                {
                    return info.index == storageIndex;
                });

    if (it == m_storageSpaceInfo.cend())
        return notExist;

    return it->effectiveSpace > 0 ? enoughSpace : notEnoughSpace;
}

}
}
