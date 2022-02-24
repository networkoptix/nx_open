// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "plugin.h"

#include <nx/kit/debug.h>
#include <nx/kit/utils.h>

#include "engine.h"
#include "stub_analytics_plugin_motion_metadata_ini.h"

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {
namespace motion_metadata {

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
    "name": "Stub: Motion Metadata",
    "description":
        "A plugin for testing and debugging Motion Metadata.",
    "version": "1.0.0",
    "vendor": "Plugin vendor"
}
)json";
}

} // namespace motion_metadata
} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
