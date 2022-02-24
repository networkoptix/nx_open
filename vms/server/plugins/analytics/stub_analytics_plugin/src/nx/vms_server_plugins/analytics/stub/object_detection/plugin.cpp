// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "plugin.h"

#include "engine.h"

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {
namespace object_detection {

using namespace nx::sdk;
using namespace nx::sdk::analytics;

Result<IEngine*> Plugin::doObtainEngine()
{
    return new Engine();
}

std::string Plugin::manifestString() const
{
    return /*suppress newline*/ 1 + (const char*)R"json(
{
    "id": "nx.stub.object_detection",
    "name": "Stub: Object Detection",
    "description": "An example Plugin for demonstrating the Base Library of Taxonomy and providing examples of object metadata generation.",
    "version": "1.0.0",
    "vendor": "Plugin vendor"
}
)json";
}

} // namespace object_detection
} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
