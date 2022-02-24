// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "../async_sql_query_executor.h"

#include <nx/utils/basic_factory.h>

namespace nx::sql {

using QueryExecutorFactoryFunction =
    std::unique_ptr<AsyncSqlQueryExecutor>(const nx::sql::ConnectionOptions& dbConnectionOptions);

class NX_SQL_API QueryExecutorProviderFactory:
    public nx::utils::BasicFactory<QueryExecutorFactoryFunction>
{
    using base_type = nx::utils::BasicFactory<QueryExecutorFactoryFunction>;

public:
    QueryExecutorProviderFactory();

    static QueryExecutorProviderFactory& instance();
};

} // namespace nx::sql
