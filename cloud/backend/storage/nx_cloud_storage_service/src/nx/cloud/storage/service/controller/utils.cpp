#include "utils.h"

namespace nx::cloud::storage::service::controller::utils {

api::ResultCode toResultCode(nx::sql::DBResult dbResult)
{
    switch (dbResult)
    {
        case nx::sql::DBResult::ok:
            return api::ResultCode::ok;
        case nx::sql::DBResult::logicError:
            return api::ResultCode::badRequest;
        case nx::sql::DBResult::notFound:
            return api::ResultCode::notFound;
        default:
            return api::ResultCode::internalError;
    }
}

api::ResultCode toResultCode(aws::ResultCode resultCode)
{
    switch (resultCode)
    {
        case aws::ResultCode::ok:
            return api::ResultCode::ok;
        case aws::ResultCode::networkError:
        case aws::ResultCode::notImplemented:
            return api::ResultCode::internalError;
        case aws::ResultCode::unauthorized:
        default:
            return api::ResultCode::awsApiError;
    }
}

api::Result toResult(const aws::Result& result)
{
    return api::Result(toResultCode(result.code()), result.text());
}

}