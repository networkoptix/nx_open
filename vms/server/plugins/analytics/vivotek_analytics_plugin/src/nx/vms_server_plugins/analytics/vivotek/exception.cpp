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

Exception::Exception(const std::system_error& systemError):
    nx::utils::Exception(QString::fromLocal8Bit(systemError.what())),
    m_errorCode(systemError.code())
{
}

Exception::Exception(std::error_code errorCode):
    nx::utils::Exception(QString::fromLocal8Bit(errorCode.message().data())),
    m_errorCode(errorCode)
{
}

std::error_code Exception::errorCode() const
{
    if (std::holds_alternative<nx::sdk::ErrorCode>(m_errorCode))
        return {};

    return std::get<std::error_code>(m_errorCode);
}

nx::sdk::Error Exception::toSdkError() const
{
    nx::sdk::ErrorCode errorCode;
    if (std::holds_alternative<nx::sdk::ErrorCode>(m_errorCode))
        errorCode = std::get<nx::sdk::ErrorCode>(m_errorCode);
    else
        errorCode = toSdk(std::get<std::error_code>(m_errorCode));

    if (errorCode == ErrorCode::noError)
        errorCode = ErrorCode::otherError;

    return error(errorCode, what());
}

} // namespace nx::vms_server_plugins::analytics::vivotek
