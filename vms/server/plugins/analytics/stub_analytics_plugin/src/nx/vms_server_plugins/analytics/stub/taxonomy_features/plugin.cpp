// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "plugin.h"

#include "engine.h"

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {
namespace taxonomy_features {

using namespace nx::sdk;
using namespace nx::sdk::analytics;

Result<IEngine*> Plugin::doObtainEngine()
{
    return new Engine(this);
}

std::string Plugin::manifestString() const
{
    return /*suppress newline*/ 1 + (const char*)R"json(
{
    "id": ")json" + instanceId() + R"json(",
    "name": "Stub, Taxonomy Features",
    "description": "An example Plugin for demonstrating features of the Analytics Taxonomy.",
    "version": "1.0.0",
    "vendor": "Plugin vendor"
}
)json";
}

} // namespace taxonomy_features
} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
