#pragma once

#include <nx/sdk/error_code.h>
#include <nx/vms/server/sdk_support/error.h>

namespace nx::sdk {

QString toString(nx::sdk::ErrorCode errorCode);

} // namespace nx::sdk

namespace nx::vms::server::sdk_support {

QString toString(const Error& error);

} // namespace nx::vms::server::sdk_support
