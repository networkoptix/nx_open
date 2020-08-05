#include "error.h"

#include <nx/utils/log/assert.h>

namespace nx::vms::server::sdk_support {

nx::vms::api::EventLevel pluginDiagnosticEventLevel(const Error& error)
{
    if (!NX_ASSERT(!error.isOk()))
        return nx::vms::api::EventLevel::UndefinedEventLevel;

    return nx::vms::api::EventLevel::ErrorEventLevel;
}

} // namespace nx::vms::server::sdk_support
