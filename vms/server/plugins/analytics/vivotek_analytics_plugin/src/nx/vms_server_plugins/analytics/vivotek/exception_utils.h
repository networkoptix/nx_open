#pragma once

#include <stdexcept>
#include <system_error>

#include <nx/sdk/result.h>
#include <nx/utils/general_macros.h>
#include <nx/utils/log/log_message.h>
#include <nx/utils/thread/cf/cfuture.h>

#include <QtCore/QString>

namespace nx::vms_server_plugins::analytics::vivotek {

constexpr auto translateBrokenPromiseToOperationCancelled =
    [](auto future)
    {
        try
        {
            return future.get();
        }
        catch (const cf::future_error& exception)
        {
            if (exception.ecode() != cf::errc::broken_promise)
                throw;
            throw std::system_error((int) std::errc::operation_canceled, std::generic_category());
        }
    };

QString collectNestedMessages(const std::exception& exception);

bool nestedContains(const std::exception& exception, std::error_condition errorCondition);

nx::sdk::Error toSdkError(const std::exception& exception);

template <typename... Args>
[[noreturn]] void rethrowWithContext(Args&&... args)
{
    std::throw_with_nested(std::runtime_error(nx::format(args...).toStdString()));
}

template <typename... Args>
auto addExceptionContext(Args&&... args)
{
    return
        [args = std::make_tuple(std::forward<Args>(args)...)](auto future)
        {
            try
            {
                return future.get();
            }
            catch (const std::exception&)
            {
                std::apply(NX_WRAP_FUNC_TO_LAMBDA(rethrowWithContext), args);
                std::terminate(); // unreachable
            }
        };
}

} // namespace nx::vms_server_plugins::analytics::vivotek
