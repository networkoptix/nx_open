#pragma once

#include <nx/vms/server/analytics/wrappers/types.h>

namespace nx::vms::server::analytics::wrappers {

std::chrono::milliseconds sdkMethodTimeout(SdkMethod method);

} // namespace nx::vms::server::analytics
