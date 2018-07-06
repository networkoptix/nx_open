#include "request_executor_factory.h"

#include <nx/utils/basic_factory.h>
#include <nx/utils/std/cpp14.h>

#include "request_execution_thread.h"

namespace nx {
namespace utils {
namespace db {

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

std::unique_ptr<BaseRequestExecutor> RequestExecutorFactory::defaultFactoryFunction(
    const ConnectionOptions& connectionOptions,
    QueryExecutorQueue* const queryExecutorQueue)
{
    return std::make_unique<DbRequestExecutionThread>(
        connectionOptions,
        queryExecutorQueue);
}

} // namespace db
} // namespace utils
} // namespace nx
