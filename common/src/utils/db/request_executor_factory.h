#pragma once

#include <nx/utils/move_only_func.h>

#include "base_request_executor.h"

namespace nx {
namespace db {

class RequestExecutorFactory
{
public:
    typedef nx::utils::MoveOnlyFunc<
        std::unique_ptr<BaseRequestExecutor>(
            const ConnectionOptions& connectionOptions,
            QueryExecutorQueue* const queryExecutorQueue)> FactoryFunc;

    static const FactoryFunc defaultFactory;

    static std::unique_ptr<BaseRequestExecutor> create(
        const ConnectionOptions& connectionOptions,
        QueryExecutorQueue* const queryExecutorQueue);

    static void setFactoryFunc(FactoryFunc factoryFunc);
};

} // namespace db
} // namespace nx
