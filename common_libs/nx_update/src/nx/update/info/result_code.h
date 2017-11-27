#pragma once

namespace nx {
namespace update {
namespace info {

enum class ResultCode
{
    ok,
    timeout,
    transportError,
    parseError,
    noData
};

} // namespace info
} // namespace update
} // namespace nx
