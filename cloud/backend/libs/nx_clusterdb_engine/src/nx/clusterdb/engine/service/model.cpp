#include "model.h"

#include "settings.h"

namespace nx::clusterdb::engine {

Model::Model(const Settings& settings)
{
    m_sqlQueryExecutor =
        std::make_unique<nx::sql::AsyncSqlQueryExecutor>(settings.db());
    if (!m_sqlQueryExecutor->init())
        throw std::runtime_error("Error connecting to DB");
}

nx::sql::AsyncSqlQueryExecutor& Model::queryExecutor()
{
    return *m_sqlQueryExecutor;
}

} // namespace nx::clusterdb::engine
