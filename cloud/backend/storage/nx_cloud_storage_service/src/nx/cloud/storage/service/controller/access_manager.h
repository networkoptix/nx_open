#pragma once

#include <string>

#include <nx/cloud/storage/service/api/result.h>
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

using AuthorizeReadingStorageHandler = nx::utils::MoveOnlyFunc<void(api::Result)>;

class AccessManager
{
public:
    AccessManager(const conf::CloudDb& settings);
    ~AccessManager();

    void stop();

    std::pair<api::Result, std::string/*accountOwner*/> authorizeAddingStorage(
        const nx::utils::stree::ResourceContainer& authInfo) const;

    void authorizeReadingStorage(
        const nx::utils::stree::ResourceContainer& authInfo,
        const api::Storage& storage,
        AuthorizeReadingStorageHandler handler);

    bool isStorageOwner(
        const nx::utils::stree::ResourceContainer& authInfo,
        const api::Storage& storage) const;

    std::string getAccountEmail(
        const nx::utils::stree::ResourceContainer& authInfo) const;

private:
    struct ReadStorageContext
    {
        std::unique_ptr<db::api::CdbClient> cdbClient;
        AuthorizeReadingStorageHandler handler;
    };

    ReadStorageContext& createReadStorageContext();
    ReadStorageContext takeReadStorageContext(db::api::CdbClient* cdbClient);

private:
    const conf::CloudDb& m_settings;
    QnMutex m_mutex;
    std::map<db::api::CdbClient*, ReadStorageContext> m_cdbRequests;
};

} // namespace controller
} // namespace storage::service
} // namespace cloud
} // namespace nx