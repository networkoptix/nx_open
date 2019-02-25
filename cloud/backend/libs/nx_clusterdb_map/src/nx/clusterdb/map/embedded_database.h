#pragma once

#include <nx/utils/uuid.h>
#include <nx/clusterdb/engine/synchronization_engine.h>

#include "database.h"

namespace nx::sql { class AsyncSqlQueryExecutor; }

namespace nx::clusterdb::map {

class Settings;

class NX_KEY_VALUE_DB_API EmbeddedDatabase
{
public:
    EmbeddedDatabase(
        const nx::clusterdb::engine::SynchronizationSettings& dataSyncSettings,
        nx::sql::AsyncSqlQueryExecutor* queryExecutor);

    Database& database();

private:
    nx::clusterdb::engine::SyncronizationEngine m_syncEngine;
    Database m_database;
};

} // namespace nx::clusterdb::map
