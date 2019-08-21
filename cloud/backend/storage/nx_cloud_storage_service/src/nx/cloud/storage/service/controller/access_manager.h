#pragma once

#include <string>

#include <nx/utils/move_only_func.h>
#include <nx/utils/thread/mutex.h>

namespace nx {

namespace utils::stree { class ResourceContainer; }

namespace cloud {

namespace db::api { class CdbClient; }

namespace storage::service {

namespace conf { struct CloudDb; }
namespace api { struct Storage; }

namespace controller {

class AccessManager
{
public:
    AccessManager(const conf::CloudDb& settings);
    ~AccessManager();

    std::pair<bool, std::string/*accountOwner*/> addStorageAllowed(
        const nx::utils::stree::ResourceContainer& authInfo) const;

    void readStorageAllowed(
        const nx::utils::stree::ResourceContainer& authInfo,
        const api::Storage& storage,
        nx::utils::MoveOnlyFunc<void(bool)> handler);

    bool isStorageOwner(
        const nx::utils::stree::ResourceContainer& authInfo,
        const api::Storage& storage);

private:
    struct ReadStorageContext
    {
        std::unique_ptr<db::api::CdbClient> cdbClient;
        nx::utils::MoveOnlyFunc<void(bool)> handler;
    };

    db::api::CdbClient* createCdbClient(
        nx::utils::MoveOnlyFunc<void(bool)> handler);
    ReadStorageContext removeCdbClient(db::api::CdbClient* cdbClient);

private:
    const conf::CloudDb& m_settings;
    QnMutex m_mutex;
    std::map<db::api::CdbClient*, ReadStorageContext> m_cdbRequests;
};

} // namespace controller
} // namespace storage::service
} // namespace cloud
} // namespace nx