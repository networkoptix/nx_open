#pragma once

#include <string>

namespace nx {
namespace data_sync_engine {

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

} // namespace data_sync_engine
} // namespace nx
