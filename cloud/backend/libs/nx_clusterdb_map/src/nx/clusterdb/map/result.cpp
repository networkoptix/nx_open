#include "result.h"

namespace nx::clusterdb::map {

const char* toString(ResultCode result)
{
    switch (result)
    {
        case ResultCode::ok:
            return "ok";
        case ResultCode::notFound:
            return "notFound";
        case ResultCode::logicError:
            return "logicError";
        case ResultCode::dbError:
            return "dbError";
        case ResultCode::unknownError:
        default:
            return "unknownError";
    }
}

Result::Result(ResultCode code, const std::string& text):
    code(code),
    text(text)
{
}

bool Result::ok() const
{
    return code == ResultCode::ok;
}

std::string nx::clusterdb::map::Result::toString() const
{
    return map::toString(code) + (!text.empty() ? (": " + text) : std::string());
}

} // nx::clusterdb::map
