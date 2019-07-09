#pragma once

#include <map>
#include <string>

#include <nx/utils/move_only_func.h>
#include <nx/utils/subscription.h>
#include <nx/utils/thread/mutex.h>

namespace nx::sql { class QueryContext; }

namespace nx::clusterdb::map {

class Database;

class NX_KEY_VALUE_DB_API Cache
{
public:
    Cache(Database* database);
    ~Cache();

    std::optional<std::string> find(const std::string& key) const;

private:
    void initialize();
    void subscribeToEvents();

private:
    Database* m_db;
    mutable QnMutex m_mutex;
    std::map<std::string, std::string> m_stringCache;

    nx::utils::SubscriptionId m_recordInsertedId;
    nx::utils::SubscriptionId m_recordRemovedId;
};

} // namespace nx::clusterdb::map