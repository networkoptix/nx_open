#pragma once

#include <string>

namespace nx::cloud::storage::service::api {

enum class ResultCode
{
    ok,
    badRequest,
    unauthorized,
    notFound,
    internalError,
    timedOut,
    networkError,
    awsApiError,
    unknownError
};

NX_NETWORK_API const char* toString(ResultCode resultCode);

struct NX_NETWORK_API Result
{
    ResultCode resultCode = ResultCode::ok;
    std::string error;

    Result(ResultCode resultCode = ResultCode::ok):
        resultCode(resultCode)
    {
    }

    Result(ResultCode resultCode, std::string error):
        resultCode(resultCode),
        error(std::move(error))
    {
    }
};

} // namespace nx::cloud::storage::service::api