// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "query_executor_factory.h"

namespace nx::sql {

QueryExecutorProviderFactory::QueryExecutorProviderFactory():
    base_type([](const nx::sql::ConnectionOptions& dbConnectionOptions) {
        return std::make_unique<AsyncSqlQueryExecutor>(dbConnectionOptions);
    })
{
}

QueryExecutorProviderFactory& QueryExecutorProviderFactory::instance()
{
    static QueryExecutorProviderFactory staticInstance;
    return staticInstance;
}

} // namespace nx::sql
