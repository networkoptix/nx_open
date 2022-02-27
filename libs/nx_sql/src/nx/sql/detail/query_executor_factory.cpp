// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "query_executor_factory.h"

#include <nx/utils/basic_factory.h>
#include <nx/utils/std/cpp14.h>

#include "query_execution_thread.h"

namespace nx::sql::detail {

RequestExecutorFactory::RequestExecutorFactory():
    base_type(
        [this](const ConnectionOptions& connection, QueryExecutorQueue* const executorQueue) {
            return RequestExecutorFactory::defaultFactoryFunction(connection, executorQueue);
        })
{
}

RequestExecutorFactory& RequestExecutorFactory::instance()
{
    static RequestExecutorFactory staticInstance;
    return staticInstance;
}

std::unique_ptr<BaseQueryExecutor> RequestExecutorFactory::defaultFactoryFunction(
    const ConnectionOptions& connectionOptions, QueryExecutorQueue* const queryExecutorQueue)
{
    return std::make_unique<QueryExecutionThread>(connectionOptions, queryExecutorQueue);
}

} // namespace nx::sql::detail
