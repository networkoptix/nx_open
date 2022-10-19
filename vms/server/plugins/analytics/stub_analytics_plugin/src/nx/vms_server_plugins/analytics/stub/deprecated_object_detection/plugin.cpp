// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "plugin.h"

#include <nx/kit/debug.h>
#include <nx/kit/utils.h>

#include "engine.h"
#include "settings_model.h"
#include "stub_analytics_plugin_deprecated_object_detection_ini.h"

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {
namespace deprecated_object_detection {

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
    "name": "Stub: Deprecated Object Detection",
    "description": "This is a deprecated plugin for testing and debugging Object Detection. If you want to generate a sequence of objects for debugging purposes, please use the Object Streamer plugin. If you want to explore the Base Library or understand how to generate and pass to VMS object metadata, please use the new Object Detection plugin.",
    "version": "1.0.0",
    "vendor": "Plugin vendor"
}
)json";
}

} // namespace deprecated_object_detection
} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
