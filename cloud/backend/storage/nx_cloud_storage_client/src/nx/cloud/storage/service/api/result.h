#pragma once

#include <string>

namespace nx::cloud::storage::service::api {

enum class ResultCode
{
    ok,
    badRequest,
    unauthorized,
	forbidden,
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

    Result(ResultCode resultCode = ResultCode::ok, std::string error = std::string()):
        resultCode(resultCode),
        error(std::move(error))
    {
    }

    bool ok() const
    {
        return resultCode == ResultCode::ok;
    }

    std::string toString() const
    {
        return std::string(api::toString(resultCode)) +
            (!error.empty() ? ": " + error : std::string());
    }
};

} // namespace nx::cloud::storage::service::api