// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "plugin.h"

#include <nx/kit/debug.h>
#include <nx/kit/utils.h>

#include "engine.h"
#include "stub_analytics_plugin_diagnostic_events_ini.h"

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {
namespace diagnostic_events {

using namespace nx::sdk;
using namespace nx::sdk::analytics;

Result<IEngine*> Plugin::doObtainEngine()
{
    return new Engine(this);
}

std::string Plugin::manifestString() const
{
    return /*suppress newline*/ 1 + (const char*)
R"json(
{
    "id": ")json" + instanceId() + R"json(",
    "name": "Stub, Plugin Diagnostic Events",
    "description": "A plugin for testing and debugging Plugin Diagnostic Events.",
    "version": "1.0.0",
    "vendor": "Plugin vendor",
    "engineSettingsModel":
    {
        "type": "Settings",
        "items":
        [
            {
                "type": "GroupBox",
                "caption": "Stub Engine settings",
                "items":
                [
                    {
                        "type": "CheckBox",
                        "name": ")json" + kGeneratePluginDiagnosticEventsFromEngineSetting + R"json(",
                        "caption": "Generate Plugin Diagnostic Events from the Engine",
                        "defaultValue": false
                    }
                ]
            }
        ]
    }
}
)json";
}

} // namespace diagnostic_events
} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
