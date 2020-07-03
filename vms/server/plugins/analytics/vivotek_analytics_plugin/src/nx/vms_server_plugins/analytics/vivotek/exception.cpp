#include "exception.h"

#include <nx/sdk/helpers/error.h>

namespace nx::vms_server_plugins::analytics::vivotek {

using namespace nx::sdk;

namespace {

ErrorCode toSdk(std::error_code errorCode)
{
    if (!errorCode)
        return ErrorCode::noError;

    if (errorCode == std::errc::connection_aborted
        || errorCode == std::errc::connection_refused
        || errorCode == std::errc::connection_reset
        || errorCode == std::errc::host_unreachable
        || errorCode == std::errc::network_down
        || errorCode == std::errc::network_reset
        || errorCode == std::errc::network_unreachable
        || errorCode == std::errc::timed_out)
    {
        return ErrorCode::networkError;
    }

    if (errorCode == std::errc::operation_not_permitted
        || errorCode == std::errc::permission_denied)
    {
        return ErrorCode::unauthorized;
    }

    return ErrorCode::otherError;
}

} // namespace

Exception::Exception(std::error_code errorCode):
    Exception(toSdk(errorCode), errorCode.message())
{
}

nx::sdk::Error Exception::toSdkError() const
{
    auto errorCode = m_errorCode;
    if (errorCode == ErrorCode::noError)
        errorCode = ErrorCode::otherError;

    return error(errorCode, what());
}

} // namespace nx::vms_server_plugins::analytics::vivotek
