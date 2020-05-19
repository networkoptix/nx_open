#pragma once

#include <tuple>
#include <exception>

#include <nx/sdk/result.h>
#include <nx/utils/general_macros.h>
#include <nx/utils/log/log_message.h>

#include <QtCore/QString>

namespace nx::vms_server_plugins::analytics::vivotek {

class Exception: public std::exception
{
public:
    explicit Exception(nx::sdk::ErrorCode errorCode, const QString& context);
    explicit Exception(std::error_code errorCode, const QString& context);
    explicit Exception(const QString& message);

    template <typename... Args>
    explicit Exception(nx::sdk::ErrorCode errorCode,
        const QString& messageFormat, const Args&... args)
    :
        Exception(errorCode, nx::format(messageFormat, args...))
    {
    }

    template <typename... Args>
    explicit Exception(std::error_code errorCode,
        const QString& contextFormat, const Args&... args)
    :
        Exception(errorCode, nx::format(contextFormat, args...))
    {
    }

    template <typename... Args>
    explicit Exception(const QString& messageFormat, const Args&... args):
        Exception(nx::format(messageFormat, args...))
    {
    }

    void addContext(const QString& context);

    template <typename... Args>
    void addContext(const QString& contextFormat, const Args&... args)
    {
        addContext(nx::format(contextFormat, args...));
    }

    virtual const char* what() const noexcept override;

    nx::sdk::Error toSdkError() const;

private:
    nx::sdk::ErrorCode m_errorCode;
    std::string m_message;
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
            catch (Exception& exception)
            {
                std::apply(
                    [&](const auto&... args) { exception.addContext(args...); },
                    args);
                throw;
            }
        };
}

} // namespace nx::vms_server_plugins::analytics::vivotek
