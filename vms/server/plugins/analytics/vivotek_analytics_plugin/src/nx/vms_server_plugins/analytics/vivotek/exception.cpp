#include "exception.h"

#include <exception>
#include <string>

#include <nx/sdk/helpers/error.h>

namespace nx::vms_server_plugins::analytics::vivotek {

using namespace std::literals;
using namespace nx::sdk;

Exception::Exception(const std::string& what, ErrorCode code):
    runtime_error(what),
    code(code)
{
}

Exception::Exception(const char* what, ErrorCode code):
    runtime_error(what),
    code(code)
{
}

namespace {
    ErrorCode getTopCode(std::exception_ptr exceptionPtr)
    {
        try
        {
            std::rethrow_exception(exceptionPtr);
        }
        catch (const Exception& exception)
        {
            return exception.code;
        }
        catch (const std::exception& exception)
        {
            try
            {
                std::rethrow_if_nested(exception);
            }
            catch (...)
            {
                return getTopCode(std::current_exception());
            }
        }

        return ErrorCode::otherError;
    }

    std::string collectMessages(const std::exception& exception)
    {
        try
        {
            std::rethrow_if_nested(exception);
        }
        catch (const std::exception& nestedException)
        {
            return exception.what() + "\ncaused by: "s + collectMessages(nestedException);
        }
        return exception.what();
    }
}

nx::sdk::Error toError(std::exception_ptr exceptionPtr)
{
    if (!exceptionPtr)
        return error(ErrorCode::noError, "");

    try
    {
        std::rethrow_exception(exceptionPtr);
    }
    catch (const std::exception& exception)
    {
        const auto errorCode = getTopCode(exceptionPtr);
        auto messages = collectMessages(exception);
        return error(errorCode, std::move(messages));
    }
}

const char* Cancelled::what() const noexcept
{
    return "Cancelled";
}

bool isCancelled(std::exception_ptr exceptionPtr)
{
    try
    {
        std::rethrow_exception(exceptionPtr);
    }
    catch (const Cancelled&)
    {
        return true;
    }
    catch (const std::exception& exception)
    {
        try
        {
            std::rethrow_if_nested(exception);
        }
        catch (...)
        {
            return isCancelled(std::current_exception());
        }
    }

    return false;
}

} // namespace nx::vms_server_plugins::analytics::vivotek
