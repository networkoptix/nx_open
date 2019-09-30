#pragma once

#include <nx/sql/db_structure_updater.h>

namespace nx::sql { class AbstractAsyncSqlQueryExecutor; }

namespace nx::cloud::storage::service::model::dao {

class StructureUpdater
{
public:
    StructureUpdater(nx::sql::AbstractAsyncSqlQueryExecutor* dbManager);

private:
    nx::sql::DbStructureUpdater m_updater;
};

} // namespace nx::cloud::storage::service::model::dao