#pragma once

#include <stdexcept>

#include <nx/sdk/result.h>
#include <nx/utils/thread/cf/cfuture.h>

namespace nx::vms_server_plugins::analytics::vivotek {

class Exception: public std::runtime_error
{
public:
    nx::sdk::ErrorCode code;

public:
    explicit Exception(const std::string& what,
        nx::sdk::ErrorCode code = nx::sdk::ErrorCode::otherError);

    explicit Exception(const char* what,
        nx::sdk::ErrorCode code = nx::sdk::ErrorCode::otherError);
};

nx::sdk::Error toError(std::exception_ptr exceptionPtr);

class Cancelled: public std::exception
{
    virtual const char* what() const noexcept override;
};

template <typename Type>
cf::future<Type> treatBrokenPromiseAsCancelled(cf::future<Type> future)
{
    return future.then(
        [](auto future)
        {
            try
            {
                return future.get();
            }
            catch (const cf::future_error& exception)
            {
                if (exception.ecode() == cf::errc::broken_promise)
                    throw Cancelled();
                throw;
            }
        });
}

bool isCancelled(std::exception_ptr exceptionPtr);

} // namespace nx::vms_server_plugins::analytics::vivotek
