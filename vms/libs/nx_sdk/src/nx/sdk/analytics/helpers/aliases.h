// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/sdk/helpers/aliases.h>
#include <nx/sdk/analytics/i_device_agent.h>
#include <nx/sdk/analytics/i_engine.h>

namespace nx {
namespace sdk {
namespace analytics {

using MutableDeviceAgentResult = Result<IDeviceAgent*>;
using MutableEngineResult = Result<IEngine*>;

} // namespace analytics
} // namespace sdk
} // namespace nx
