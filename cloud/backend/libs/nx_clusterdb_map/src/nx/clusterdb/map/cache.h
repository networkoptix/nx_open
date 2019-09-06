#pragma once

#include <map>
#include <string>

#include <nx/utils/move_only_func.h>
#include <nx/utils/subscription.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/async_operation_guard.h>

namespace nx::sql { class QueryContext; }

namespace nx::clusterdb::map {

class Database;

class NX_KEY_VALUE_DB_API Cache
{
public:
    Cache(Database* database);
    ~Cache();

    std::optional<std::string> find(const std::string& key) const;

    std::map<std::string, std::string> getRange(
        const std::string& lowerBound,
        const std::string& upperBound) const;

    std::map<std::string, std::string> getRange(const std::string& lowerBound) const;

    std::map<std::string, std::string> getRangeWithPrefix(const std::string& prefix) const;

private:
    void initialize();
    void subscribeToEvents();

private:
    Database* m_db;
    mutable QnMutex m_mutex;
    std::map<std::string, std::string> m_stringCache;

    nx::utils::SubscriptionId m_recordInsertedId = nx::utils::kInvalidSubscriptionId;
    nx::utils::SubscriptionId m_recordRemovedId = nx::utils::kInvalidSubscriptionId;
};

} // namespace nx::clusterdb::map