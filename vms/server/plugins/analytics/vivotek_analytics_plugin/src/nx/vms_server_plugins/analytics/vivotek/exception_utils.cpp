#include "exception_utils.h"
#include "nx/utils/system_error.h"

#include <nx/sdk/helpers/error.h>
#include <nx/utils/log/log_message.h>

namespace nx::vms_server_plugins::analytics::vivotek {

using namespace nx::sdk;

namespace {

template <typename Callback>
void enumerateNested(const std::exception& exception, Callback callback)
{
    callback(exception);
    try
    {
        std::rethrow_if_nested(exception);
    }
    catch (const std::exception& nested)
    {
        enumerateNested(nested, std::move(callback));
    }
}

std::error_code findFirstNestedErrorCode(const std::exception& exception)
{
    std::error_code errorCode;
    enumerateNested(exception,
        [&](const auto& exception)
        {
            if (errorCode)
                return;

            if (const auto e = dynamic_cast<std::system_error const*>(&exception))
                errorCode = e->code();
        });
    return errorCode;
}

ErrorCode translateToSdk(std::error_code errorCode)
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
        return ErrorCode::networkError;

    if (errorCode == std::errc::operation_not_permitted
     || errorCode == std::errc::permission_denied)
        return ErrorCode::unauthorized;

    return ErrorCode::otherError;
}

} // namespace

QString collectNestedMessages(const std::exception& exception)
{
    QString messages;
    enumerateNested(exception,
        [&](const auto& exception)
        {
            if (!messages.isEmpty())
                messages += "\ncaused by\n";

            messages += exception.what();
        });
    return messages;
}

bool nestedContains(const std::exception& exception, std::error_condition errorCondition)
{
    bool contains = false;
    enumerateNested(exception,
        [&](const auto& exception)
        {
            if (contains)
                return;

            if (const auto e = dynamic_cast<std::system_error const*>(&exception))
                contains = e->code() == errorCondition;
        });
    return contains;
}

Error toSdkError(const std::exception& exception)
{
    auto errorCode = translateToSdk(findFirstNestedErrorCode(exception));
    if (errorCode == ErrorCode::noError)
        errorCode = ErrorCode::otherError;

    return error(errorCode, collectNestedMessages(exception).toStdString());
}

} // namespace nx::vms_server_plugins::analytics::vivotek
