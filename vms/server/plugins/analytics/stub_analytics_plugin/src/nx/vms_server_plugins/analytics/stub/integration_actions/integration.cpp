// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "integration.h"

#include "engine.h"

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {
namespace integration_actions {

using namespace nx::sdk;
using namespace nx::sdk::analytics;

Result<IEngine*> Integration::doObtainEngine()
{
    return new Engine();
}

std::string Integration::manifestString() const
{
    return /*suppress newline*/ 1 + (const char*)
R"json(
{
    "id": "nx.stub.integration_actions",
    "name": "Stub, Integration Actions",
    "description": "A plugin for testing and debugging Integration Actions.",
    "version": "1.0.0",
    "vendor": "Plugin vendor"
}
)json";
}

} // namespace integration_actions
} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
