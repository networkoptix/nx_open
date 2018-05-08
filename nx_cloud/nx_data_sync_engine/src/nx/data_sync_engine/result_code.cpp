#include "result_code.h"

namespace nx {
namespace data_sync_engine {

std::string toString(ResultCode resultCode)
{
    switch (resultCode)
    {
        case ResultCode::ok:
            return "ok";
        case ResultCode::partialContent:
            return "partialContent";
        case ResultCode::dbError:
            return "dbError";
        case ResultCode::retryLater:
            return "retryLater";
        case ResultCode::notFound:
            return "notFound";
        case ResultCode::badRequest:
            return "badRequest";
        default:
            return "unknownError";
    }
}

} // namespace data_sync_engine
} // namespace nx
