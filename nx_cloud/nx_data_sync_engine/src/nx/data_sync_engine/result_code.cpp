#include "result_code.h"

namespace nx {
namespace cdb {
namespace ec2 {

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

} // namespace ec2
} // namespace cdb
} // namespace nx
