#pragma once

#include <nx/utils/move_only_func.h>

#include "base_request_executor.h"

namespace nx {
namespace utils {
namespace db {

class RequestExecutorFactory
{
public:
    typedef nx::utils::MoveOnlyFunc<
        std::unique_ptr<BaseRequestExecutor>(
            const ConnectionOptions& connectionOptions,
            QueryExecutorQueue* const queryExecutorQueue)> FactoryFunc;

    static std::unique_ptr<BaseRequestExecutor> create(
        const ConnectionOptions& connectionOptions,
        QueryExecutorQueue* const queryExecutorQueue);

    static void setFactoryFunc(FactoryFunc factoryFunc);
};

} // namespace db
} // namespace utils
} // namespace nx
