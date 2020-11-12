#pragma once

#include <utility>
#include <variant>
#include <system_error>

#include <nx/utils/exception.h>
#include <nx/sdk/result.h>
#include <nx/sdk/helpers/error.h>

#include <QtCore/QString>

namespace nx::vms_server_plugins::utils {

class Exception: public nx::utils::Exception
{
public:
    using nx::utils::Exception::Exception;

    template <typename... Args>
    explicit Exception(nx::sdk::ErrorCode errorCode, Args&&... args):
        nx::utils::Exception(std::forward<Args>(args)...),
        m_errorCode(errorCode)
    {
    }

    explicit Exception(const std::system_error& systemError);

    explicit Exception(std::error_code errorCode);

    template <typename... Args>
    explicit Exception(std::error_code errorCode, const Args&... args):
        Exception(errorCode)
    {
        addContext(args...);
    }

    std::error_code errorCode() const;

    nx::sdk::Error toSdkError() const;

private:
    std::variant<std::error_code, nx::sdk::ErrorCode> m_errorCode;
};

const auto translateSystemError =
    [](auto future)
    {
        try
        {
            return future.get();
        }
        catch (const std::system_error& exception)
        {
            throw Exception(exception);
        }
    };

template <typename Value, typename Function>
void interceptExceptions(nx::sdk::Result<Value>* outResult, Function&& function) noexcept
{
    try
    {
        if constexpr(std::is_void_v<Value>)
        {
            std::forward<Function>(function)();
            *outResult = {};
        }
        else
        {
            *outResult = std::forward<Function>(function)();
        }
    }
    catch (const Exception& exception)
    {
        *outResult = exception.toSdkError();
    }
    catch (const std::exception& exception)
    {
        *outResult = error(nx::sdk::ErrorCode::internalError, exception.what());
    }
    catch (...)
    {
        *outResult = error(nx::sdk::ErrorCode::internalError, "Unknown exception occurred");
    }
}

} // namespace nx::vms_server_plugins::utils
