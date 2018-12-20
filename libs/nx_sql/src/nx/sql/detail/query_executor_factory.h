#pragma once

#include <nx/utils/basic_factory.h>
#include <nx/utils/move_only_func.h>

#include "base_query_executor.h"

namespace nx::sql::detail {

using CreateQueryExecutorFunc =
    std::unique_ptr<BaseQueryExecutor>(
        const ConnectionOptions& connectionOptions,
        QueryExecutorQueue* const queryExecutorQueue);

class NX_SQL_API RequestExecutorFactory:
    public nx::utils::BasicFactory<CreateQueryExecutorFunc>
{
    using base_type = nx::utils::BasicFactory<CreateQueryExecutorFunc>;

public:
    RequestExecutorFactory();

    static RequestExecutorFactory& instance();

private:
    std::unique_ptr<BaseQueryExecutor> defaultFactoryFunction(
        const ConnectionOptions& connectionOptions,
        QueryExecutorQueue* const queryExecutorQueue);
};

} // namespace nx::sql::detail
