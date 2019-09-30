#pragma once

#include <nx/utils/uuid.h>
#include <nx/clusterdb/engine/synchronization_engine.h>

#include "cache.h"
#include "database.h"

namespace nx::sql { class AsyncSqlQueryExecutor; }

namespace nx::clusterdb::map {

class Settings;

class NX_KEY_VALUE_DB_API EmbeddedDatabase
{
public:
    EmbeddedDatabase(
        const Settings& settings,
        nx::sql::AsyncSqlQueryExecutor* queryExecutor);

    Database& database();

    /**
     * Get a pointer to this map's local in memory cache of all key/value pairs in the db.
     * NOTE: returns nullptr if Settings::enableCache is false
     */
    Cache* cache();

    nx::clusterdb::engine::SynchronizationEngine& synchronizationEngine();
    const nx::clusterdb::engine::SynchronizationEngine& synchronizationEngine() const;

private:
    nx::clusterdb::engine::SynchronizationEngine m_syncEngine;
    Database m_database;
    std::unique_ptr<Cache> m_cache;
};

} // namespace nx::clusterdb::map
