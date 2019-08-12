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

NX_CLOUD_STORAGE_CLIENT_API const char* toString(ResultCode resultCode);

struct NX_CLOUD_STORAGE_CLIENT_API Result
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