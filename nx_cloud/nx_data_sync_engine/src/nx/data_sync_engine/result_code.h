#pragma once

#include <string>

namespace nx {
namespace cdb {
namespace ec2 {

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

} // namespace ec2
} // namespace cdb
} // namespace nx
