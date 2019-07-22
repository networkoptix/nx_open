#include "to_string.h"

#include <nx/sdk/helpers/to_string.h>

namespace nx::sdk {

QString toString(nx::sdk::ErrorCode errorCode)
{
    return QString::fromStdString(nx::sdk::toStdString(errorCode));
}

} // namespace nx::sdk

namespace nx::vms::server::sdk_support {

QString toString(const Error& error)
{
    return lm("[%1]: %2").args(
        error.errorCode,
        error.errorMessage.isEmpty() ? QString("no error message") : error.errorMessage);
}

} // namespace nx::vms::server::sdk_support
