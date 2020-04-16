#pragma once

#include <cstdint>
#include <string_view>

namespace nx::vms_server_plugins::analytics::vivotek {

std::int64_t parseIsoTimestamp(std::string_view isoTimestamp);

} // namespace nx::vms_server_plugins::analytics::vivotek
