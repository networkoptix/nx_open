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

}