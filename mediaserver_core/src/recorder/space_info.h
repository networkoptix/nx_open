#pragma once

#include <vector>
#include <functional>
#include <random>
#include <nx/utils/thread/mutex.h>

namespace nx {
namespace recorder {

class SpaceInfo
{
public:
    struct StorageSpaceInfo
    {
        int index;
        int64_t effectiveSpace;
        int64_t totalSpace;

        StorageSpaceInfo() : index(0), effectiveSpace(0LL), totalSpace(0LL) {}
        StorageSpaceInfo(int index, int64_t totalSpace) :
            index(index),
            effectiveSpace(-1LL),
            totalSpace(totalSpace) {}

        bool isEffectiveSpaceSet() const { return effectiveSpace == -1; }
    };

    using SpaceInfoVector = std::vector<StorageSpaceInfo>;

    enum RecordingReadinessState
    {
        enoughSpace,
        notEnoughSpace,
        notExist
    };

    SpaceInfo();
    void storageAdded(int index, int64_t totalSpace);
    void storageRemoved(int index);
    void storageChanged(int index, int64_t freeSpace, int64_t nxOccupiedSpace, int64_t spaceLimit);

    /**
    * Returns optimal storage index or -1 if this index can't be determined
    */
    int getOptimalStorageIndex(const std::vector<int>& allowedIndexes) const;
    RecordingReadinessState state(int storageIndex) const;

private:
    SpaceInfoVector::iterator storageByIndex(int storageIndex);
    int getStorageIndexImpl(const SpaceInfoVector& filteredSpaceInfo, bool byEffectiveSpace) const;

private:
    SpaceInfoVector m_storageSpaceInfo;
    std::random_device m_rd;
    mutable std::mt19937 m_gen;
    mutable QnMutex m_mutex;
};

}
}
