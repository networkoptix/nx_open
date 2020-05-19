#include "exception.h"

#include <nx/sdk/helpers/error.h>

namespace nx::vms_server_plugins::analytics::vivotek {

using namespace nx::sdk;

namespace {

std::string prependMessageContext(const std::string& context, const std::string& message)
{
    return context + "\ncaused by\n" + message;
}

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
        || errorCode == std::errc::network_unreachable)
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

Exception::Exception(nx::sdk::ErrorCode errorCode, const QString& message):
    m_errorCode(errorCode),
    m_message(message.toStdString())
{
}

Exception::Exception(std::error_code errorCode, const QString& context):
    m_errorCode(toSdk(errorCode)),
    m_message(prependMessageContext(context.toStdString(), errorCode.message()))
{
}

Exception::Exception(const QString& message):
    Exception(nx::sdk::ErrorCode::otherError, message)
{
}

void Exception::addContext(const QString& context)
{
    m_message = prependMessageContext(context.toStdString(), m_message);
}

const char* Exception::what() const noexcept
{
    return m_message.data();
}

nx::sdk::Error Exception::toSdkError() const
{
    auto errorCode = m_errorCode;
    if (errorCode == ErrorCode::noError)
        errorCode = ErrorCode::otherError;

    return error(errorCode, m_message);
}

} // namespace nx::vms_server_plugins::analytics::vivotek
