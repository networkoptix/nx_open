#include "error.h"

#include <nx/utils/log/assert.h>
#include <nx/fusion/model_functions.h>
#include <nx/sdk/helpers/to_string.h>

namespace nx::vms::server::sdk_support {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(Error, (json), (errorMessage)(errorCode))

nx::vms::api::EventLevel pluginDiagnosticEventLevel(const Error& error)
{
    if (!NX_ASSERT(!error.isOk()))
        return nx::vms::api::EventLevel::UndefinedEventLevel;

    return nx::vms::api::EventLevel::ErrorEventLevel;
}

QString Error::toString() const
{
    return NX_FMT("[%1]: %2",
        errorCode,
        errorMessage.isEmpty() ? "no error message" : errorMessage);
}

} // namespace nx::vms::server::sdk_support

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(
    nx::sdk, ErrorCode,
    (nx::sdk::ErrorCode::noError, "noError")
    (nx::sdk::ErrorCode::networkError, "networkError")
    (nx::sdk::ErrorCode::unauthorized, "unauthorized")
    (nx::sdk::ErrorCode::internalError, "internalError")
    (nx::sdk::ErrorCode::notImplemented, "notImplemented")
    (nx::sdk::ErrorCode::otherError, "otherError"))
