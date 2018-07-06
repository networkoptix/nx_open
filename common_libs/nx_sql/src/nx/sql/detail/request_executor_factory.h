#pragma once

#include <nx/utils/basic_factory.h>
#include <nx/utils/move_only_func.h>

#include "base_request_executor.h"

namespace nx::sql::detail {

using CreateRequestExecutorFunc =
    std::unique_ptr<BaseRequestExecutor>(
        const ConnectionOptions& connectionOptions,
        QueryExecutorQueue* const queryExecutorQueue);

class NX_SQL_API RequestExecutorFactory:
    public nx::utils::BasicFactory<CreateRequestExecutorFunc>
{
    using base_type = nx::utils::BasicFactory<CreateRequestExecutorFunc>;

public:
    RequestExecutorFactory();

    static RequestExecutorFactory& instance();

private:
    std::unique_ptr<BaseRequestExecutor> defaultFactoryFunction(
        const ConnectionOptions& connectionOptions,
        QueryExecutorQueue* const queryExecutorQueue);
};

} // namespace nx::sql::detail
