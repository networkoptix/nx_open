#pragma once

#include <vector>
#include <core/resource/resource_fwd.h>
#include <nx/vms/server/resource/storage_resource.h>

class QnStorageManager;

namespace nx::vms::server {

class WritableStoragesHelper
{
public:
    WritableStoragesHelper(const QnStorageManager* owner);
    StorageResourceList list(const StorageResourceList& storages) const;
    StorageResourcePtr optimalStorageForRecording(const StorageResourceList& storages) const;
    static StorageResourceList filterOutSmall(const StorageResourceList& storages);

private:
    struct SpaceInfo
    {
        int64_t totalSpace = -1LL;
        int64_t freeSpace = -1LL;
        int64_t spaceLimit = -1LL;
        int64_t nxOccupied = -1LL;
        int64_t available = -1LL;
        int64_t effective = -1LL;
        std::string url;
        bool isSystem = false;
        StorageResourcePtr storage;

        static SpaceInfo from(const StorageResourcePtr& storage, const QnStorageManager* owner);
        static StorageResourceList toStorageList(const std::vector<SpaceInfo>& infos);
        std::string toString() const;
        static int64_t getOrThrow(
            int64_t value, const std::string& description, const std::string& url);
    };

    const QnStorageManager* m_owner;

    std::vector<SpaceInfo> resultInfos(const std::vector<SpaceInfo>& candidates) const;
    static std::vector<SpaceInfo> online(const std::vector<SpaceInfo>& infos);
    static StorageResourceList unique(const StorageResourceList& candidates);
    static std::vector<SpaceInfo> filterOutSpaceless(const std::vector<SpaceInfo>& infos);
    static std::vector<SpaceInfo> filterOutSmall(const std::vector<SpaceInfo>& infos);
    static std::vector<SpaceInfo> filterOutSmallSystem(const std::vector<SpaceInfo>& infos);
    static std::vector<SpaceInfo> filterOutUnused(const std::vector<SpaceInfo>& infos);
    static void adjustFlags(
        const StorageResourceList& candidates, const StorageResourceList& result);
    static std::vector<SpaceInfo> toInfos(
        const StorageResourceList& storages, const QnStorageManager *owner);
};

} // namespace nx::vms::server
