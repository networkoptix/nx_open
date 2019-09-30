#include "result.h"

namespace nx::cloud::storage::service::api {

const char* toString(ResultCode resultCode)
{
    switch (resultCode)
    {
        case ResultCode::ok:
            return "ok";
        case ResultCode::badRequest:
            return "badRequest";
        case ResultCode::unauthorized:
            return "unauthorized";
		case ResultCode::forbidden:
			return "forbidden";
        case ResultCode::notFound:
            return "notFound";
        case ResultCode::internalError:
            return "internalError";
        case ResultCode::timedOut:
            return "timedOut";
        case ResultCode::networkError:
            return "networkError";
        case ResultCode::awsApiError:
            return "awsApiError";
        case ResultCode::unknownError:
        default:
            return "unknownError";
    }
}

} // namespace nx::cloud::storage::service::api