#pragma once

#include <nx/vms/server/sdk_support/result_holder.h>
#include <nx/vms/api/types/event_rule_types.h>
#include <nx/fusion/model_functions_fwd.h>

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

    bool isOk() const { return errorCode == nx::sdk::ErrorCode::noError; }
};

QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(nx::sdk::ErrorCode)
QN_FUSION_DECLARE_FUNCTIONS(Error, (json))

nx::vms::api::EventLevel pluginDiagnosticEventLevel(const Error& error);

} // namespace nx::vms::server::sdk_support

Q_DECLARE_METATYPE(nx::vms::server::sdk_support::Error)