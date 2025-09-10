// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "result_code.h"

namespace nx::cloud::db::api {

bool Result::isTechnicalError()
{
    return code == ResultCode::dbError
        || code == ResultCode::networkError
        || code == ResultCode::notImplemented
        || code == ResultCode::serviceUnavailable
        || code == ResultCode::retryLater;
}

bool Result::isBusinessLogicError()
{
    return !ok() && !isTechnicalError();
}

bool isSuccessCode(ResultCode code)
{
    return code == ResultCode::ok
        || code == ResultCode::created
        || code == ResultCode::partialContent
        || code == ResultCode::noContent;
}

} // namespace nx::cloud::db::api
