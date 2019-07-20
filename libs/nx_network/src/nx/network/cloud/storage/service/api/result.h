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
    unknownError
};

struct NX_NETWORK_API Result
{
    ResultCode resultCode;
    std::string error;
};

} // namespace nx::cloud::storage::service::api