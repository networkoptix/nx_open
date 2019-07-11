// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/sdk/helpers/aliases.h>
#include <nx/sdk/analytics/i_device_agent.h>
#include <nx/sdk/analytics/i_engine.h>

namespace nx {
namespace sdk {
namespace analytics {

using DeviceAgentResult = nx::sdk::Result<nx::sdk::analytics::IDeviceAgent*>;
using EngineResult = nx::sdk::Result<nx::sdk::analytics::IEngine*>;

} // namespace analytics
} // namespace sdk
} // namespace nx
