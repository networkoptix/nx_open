#pragma once

#include <vector>
#include <core/resource/resource_fwd.h>

class QnStorageManager;

namespace nx::vms::server {

class WritableStoragesHelper
{
public:
    WritableStoragesHelper(const QnStorageManager* owner);
    QnStorageResourceList list(const QnStorageResourceList& storages) const;
    QnStorageResourcePtr optimalStorageForRecording(const QnStorageResourceList& storages) const;

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
        QnStorageResourcePtr storage;

        static SpaceInfo from(const QnStorageResourcePtr& storage, const QnStorageManager* owner);
        static QnStorageResourceList toStorageList(const std::vector<SpaceInfo>& infos);
        std::string toString() const;
        static int64_t getOrThrow(
            int64_t value, const std::string& description, const std::string& url);
    };

    const QnStorageManager* m_owner;

    std::vector<SpaceInfo> resultInfos(const std::vector<SpaceInfo>& candidates) const;
    std::vector<SpaceInfo> online(const QnStorageResourceList& storages) const;
    static QnStorageResourceList unique(const QnStorageResourceList& candidates);
    static std::vector<SpaceInfo> filterOutSpaceless(const std::vector<SpaceInfo>& infos);
    static std::vector<SpaceInfo> filterOutSmall(const std::vector<SpaceInfo>& infos);
    static std::vector<SpaceInfo> filterOutSmallSystem(const std::vector<SpaceInfo>& infos);
    static std::vector<SpaceInfo> filterOutUnused(const std::vector<SpaceInfo>& infos);
    static void adjustFlags(
        const QnStorageResourceList& candidates, const QnStorageResourceList& result);
};

} // namespace nx::vms::server
