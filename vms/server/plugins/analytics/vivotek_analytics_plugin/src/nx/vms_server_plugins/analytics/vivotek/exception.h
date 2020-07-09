#pragma once

#include <tuple>
#include <exception>
#include <variant>

#include <nx/sdk/result.h>
#include <nx/utils/exception.h>
#include <nx/utils/log/log_message.h>

#include <QtCore/QString>

namespace nx::vms_server_plugins::analytics::vivotek {

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

template <typename... Args>
auto addExceptionContextAndRethrow(Args&&... args)
{
    return
        [args = std::make_tuple(std::forward<Args>(args)...)](auto future)
        {
            try
            {
                return future.get();
            }
            catch (nx::utils::Exception& exception)
            {
                std::apply(
                    [&](const auto&... args) { exception.addContext(args...); },
                    args);
                throw;
            }
        };
}

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

} // namespace nx::vms_server_plugins::analytics::vivotek
