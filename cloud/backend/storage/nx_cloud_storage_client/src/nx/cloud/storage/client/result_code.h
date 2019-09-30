#pragma once

#include <string_view>
#include <iostream>

namespace nx::cloud::storage::client {

enum class ResultCode
{
    ok = 0,
    unauthorized,
    ioError,
    notImplemented,
};

constexpr std::string_view toString(ResultCode code)
{
    switch (code)
    {
        case ResultCode::ok:
            return "ok";
        case ResultCode::unauthorized:
            return "unauthorized";
        case ResultCode::ioError:
            return "ioError";
        case ResultCode::notImplemented:
            return "notImplemented";
    }

    return "unknown";
}

inline std::ostream& operator<<(std::ostream& s, ResultCode resultCode)
{
    return s << toString(resultCode);
}

} // namespace nx::cloud::storage::client
