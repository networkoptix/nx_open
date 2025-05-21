// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <string>

namespace nx::telemetry {

class Settings;

struct InitAttributes
{
    std::string serviceName;
    std::string serviceVersion;
};

NX_TELEMETRY_API void init(const InitAttributes& attributes, const Settings& settings);

} // namespace nx::telemetry
