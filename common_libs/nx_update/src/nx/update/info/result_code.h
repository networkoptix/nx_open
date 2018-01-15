#pragma once

namespace nx {
namespace update {
namespace info {

enum class ResultCode
{
    ok,
    timeout,
    getRawDataError,
    parseError,
    noData
};

NX_UPDATE_API QString toString(ResultCode resultCode);

} // namespace info
} // namespace update
} // namespace nx
