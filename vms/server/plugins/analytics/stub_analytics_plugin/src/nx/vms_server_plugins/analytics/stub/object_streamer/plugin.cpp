// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "plugin.h"

#include "engine.h"

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {
namespace object_streamer {

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
    "id": "nx.stub.object_streamer",
    "name": "Stub, Object Streamer",
    "description": "A plugin for generating Objects from a prepared JSON file (the format is described in the readme.md file of this Plugin).",
    "version": "1.0.0",
    "vendor": "Plugin vendor"
}
)json";
}

} // namespace object_streamer
} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
