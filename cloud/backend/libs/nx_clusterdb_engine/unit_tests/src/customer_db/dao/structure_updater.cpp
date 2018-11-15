#include "structure_updater.h"

namespace nx::clusterdb::engine::test::dao {

static constexpr char kSchemaName[] = 
    "customer_db_b3ad7d30-bdb6-492e-8be5-aeb1bdaf9c1f";

static constexpr char kInitialDbStructure[] = R"sql(
CREATE TABLE customer(
    id                  VARCHAR(64) NOT NULL PRIMARY KEY,
    full_name           VARCHAR(1024),
    address             VARCHAR(1024)
);

)sql";

StructureUpdater::StructureUpdater(
    nx::sql::AbstractAsyncSqlQueryExecutor* const dbManager)
    :
    m_updater(kSchemaName, dbManager)
{
    // Registering update scripts.
    m_updater.addUpdateScript(kInitialDbStructure);

    // Performing update.
    if (!m_updater.updateStructSync())
        throw std::runtime_error("Failed to update DB structure");
}

} // namespace nx::clusterdb::engine::test::dao
