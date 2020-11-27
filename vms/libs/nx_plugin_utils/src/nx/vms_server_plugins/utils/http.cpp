#include "http.h"

#include <nx/sdk/result.h>

#include "exception.h"

namespace nx::vms_server_plugins::utils {

using namespace nx::network;
using namespace nx::sdk;

void verifySuccessOrThrow(http::StatusCode::Value status)
{
    ErrorCode error = ErrorCode::noError;

    switch (status)
    {
        case http::StatusCode::unauthorized:
        case http::StatusCode::forbidden:
            error = ErrorCode::unauthorized;
            break;

        default:
            if (!http::StatusCode::isSuccessCode(status))
                error = ErrorCode::otherError;
    }

    if (error != ErrorCode::noError)
        throw Exception(error, "HTTP response status code %1", status);
}

void verifySuccessOrThrow(const http::StatusLine& line)
{
    verifySuccessOrThrow(static_cast<http::StatusCode::Value>(line.statusCode));
}

void verifySuccessOrThrow(const http::Response& response)
{
    verifySuccessOrThrow(response.statusLine);
}

} // namespace nx::vms_server_plugins::utils
