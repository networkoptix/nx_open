#pragma once

#include <nx/vms/server/sdk_support/result_holder.h>

namespace nx::vms::server::sdk_support {

struct Error
{
    nx::sdk::ErrorCode errorCode = nx::sdk::ErrorCode::noError;
    QString errorMessage;

    template<typename Value>
    static Error fromResultHolder(const ResultHolder<Value>& result)
    {
        Error error;
        error.errorCode = result.errorCode();
        const auto errorMessage = result.errorMessage();
        if (!errorMessage.isEmpty())
            error.errorMessage = errorMessage;

        return error;
    }
};

} // namespace nx::vms::server::sdk_support
