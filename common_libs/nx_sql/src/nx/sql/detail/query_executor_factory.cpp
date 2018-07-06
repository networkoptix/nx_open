#include "query_executor_factory.h"

#include <nx/utils/basic_factory.h>
#include <nx/utils/std/cpp14.h>

#include "query_execution_thread.h"

namespace nx::sql::detail {

RequestExecutorFactory::RequestExecutorFactory():
    base_type(std::bind(&RequestExecutorFactory::defaultFactoryFunction, this,
        std::placeholders::_1, std::placeholders::_2))
{
}

RequestExecutorFactory& RequestExecutorFactory::instance()
{
    static RequestExecutorFactory staticInstance;
    return staticInstance;
}

std::unique_ptr<BaseQueryExecutor> RequestExecutorFactory::defaultFactoryFunction(
    const ConnectionOptions& connectionOptions,
    QueryExecutorQueue* const queryExecutorQueue)
{
    return std::make_unique<QueryExecutionThread>(
        connectionOptions,
        queryExecutorQueue);
}

} // namespace nx::sql::detail
