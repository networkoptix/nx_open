#include "request_executor_factory.h"

#include <nx/utils/std/cpp14.h>

#include "request_execution_thread.h"

namespace nx {
namespace utils {
namespace db {

static RequestExecutorFactory::FactoryFunc actualFactory;

std::unique_ptr<BaseRequestExecutor> RequestExecutorFactory::create(
    const ConnectionOptions& connectionOptions,
    QueryExecutorQueue* const queryExecutorQueue)
{
    if (actualFactory)
        return actualFactory(connectionOptions, queryExecutorQueue);

    return std::make_unique<DbRequestExecutionThread>(
        connectionOptions,
        queryExecutorQueue);
}

void RequestExecutorFactory::setFactoryFunc(
    RequestExecutorFactory::FactoryFunc factoryFunc)
{
    actualFactory = std::move(factoryFunc);
}

} // namespace db
} // namespace utils
} // namespace nx
