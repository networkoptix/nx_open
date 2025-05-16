// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <string_view>

namespace nx::telemetry {

class Settings;

NX_TELEMETRY_API void init(const std::string_view& serviceName, const Settings& settings);

} // namespace nx::telemetry
