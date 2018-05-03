#pragma once

#include <nx/utils/basic_factory.h>
#include <nx/utils/move_only_func.h>

#include "base_request_executor.h"

namespace nx {
namespace utils {
namespace db {

using CreateRequestExecutorFunc =
    std::unique_ptr<BaseRequestExecutor>(
        const ConnectionOptions& connectionOptions,
        QueryExecutorQueue* const queryExecutorQueue);

class NX_UTILS_API RequestExecutorFactory:
    public BasicFactory<CreateRequestExecutorFunc>
{
    using base_type = BasicFactory<CreateRequestExecutorFunc>;

public:
    RequestExecutorFactory();

    static RequestExecutorFactory& instance();

private:
    std::unique_ptr<BaseRequestExecutor> defaultFactoryFunction(
        const ConnectionOptions& connectionOptions,
        QueryExecutorQueue* const queryExecutorQueue);
};

} // namespace db
} // namespace utils
} // namespace nx
