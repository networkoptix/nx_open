// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <nx/kit/debug.h>
#include <nx/kit/utils.h>

#include "plugin.h"
#include "engine.h"
#include "stub_analytics_plugin_events_ini.h"

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {
namespace events {

using namespace nx::sdk;
using namespace nx::sdk::analytics;

Result<IEngine*> Plugin::doObtainEngine()
{
    return new Engine(this);
}

std::string Plugin::manifestString() const
{
    return /*suppress newline*/ 1 + (const char*) R"json(
{
    "id": ")json" + instanceId() + R"json(",
    "name": "Stub, Events",
    "description": "A plugin for testing and debugging Events.",
    "version": "1.0.0",
    "vendor": "Plugin vendor"
}
)json";
}

} // namespace events
} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
