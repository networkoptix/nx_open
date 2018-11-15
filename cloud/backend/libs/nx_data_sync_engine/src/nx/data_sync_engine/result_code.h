#pragma once

#include <string>

namespace nx::clusterdb::engine {

enum class ResultCode
{
    ok = 0,
    partialContent,
    dbError,
    retryLater,
    notFound,
    badRequest,
};

std::string toString(ResultCode);

} // namespace nx::clusterdb::engine
