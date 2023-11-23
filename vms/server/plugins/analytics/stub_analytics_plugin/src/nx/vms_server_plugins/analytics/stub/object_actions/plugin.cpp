// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "plugin.h"

#include "engine.h"

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {
namespace object_actions {

using namespace nx::sdk;
using namespace nx::sdk::analytics;

Result<IEngine*> Plugin::doObtainEngine()
{
    return new Engine();
}

std::string Plugin::manifestString() const
{
    return /*suppress newline*/ 1 + (const char*)
R"json(
{
    "id": "nx.stub.object_actions",
    "name": "Stub, Object Actions",
    "description": "A plugin for testing and debugging Object Actions.",
    "version": "1.0.0",
    "vendor": "Plugin vendor"
}
)json";
}

} // namespace object_actions
} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
