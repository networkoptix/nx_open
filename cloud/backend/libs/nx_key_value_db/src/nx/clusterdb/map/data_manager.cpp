#include "data_manager.h"

#include <nx/sql/async_sql_query_executor.h>

namespace nx::clusterdb::map {

DataManager::DataManager(
    nx::sql::AsyncSqlQueryExecutor* /*dbManager*/)
    /*:
    m_dbManager(dbManager)*/
{
}

void DataManager::set(
    const std::string& /*key*/,
    const std::string& /*value*/,
    UpdateCompletionHander /*completionHander*/)
{
    // TODO
}

void DataManager::remove(
    const std::string& /*key*/,
    UpdateCompletionHander /*completionHander*/)
{
    // TODO
}

void DataManager::get(
    const std::string& /*key*/,
    LookupCompletionHander /*completionHander*/)
{
    // TODO
}

void DataManager::set(
    nx::sql::QueryContext* /*queryContext*/,
    const std::string& /*key*/,
    const std::string& /*value*/)
{
    // TODO
}

void DataManager::remove(
    nx::sql::QueryContext* /*queryContext*/,
    const std::string& /*key*/)
{
    // TODO
}

std::optional<std::string> DataManager::get(
    nx::sql::QueryContext* /*queryContext*/,
    const std::string& /*key*/)
{
    // TODO
    return std::nullopt;
}

} // namespace nx::clusterdb::map
