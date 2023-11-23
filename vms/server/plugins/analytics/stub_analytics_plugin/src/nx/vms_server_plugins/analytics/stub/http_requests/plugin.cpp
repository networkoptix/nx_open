// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "plugin.h"

#include "engine.h"

namespace nx::vms_server_plugins::analytics::stub::http_requests {

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
    "id": "nx.stub.http_requests",
    "name": "Stub, HTTP Requests",
    "description": "A plugin for testing and debugging HTTP requests from the Plugin to the Cloud or VMS Server.",
    "version": "1.0.0",
    "vendor": "Plugin vendor"
}
)json";
}

nx::sdk::Ptr<nx::sdk::IUtilityProvider> Plugin::utilityProvider() const
{
    return nx::sdk::analytics::Plugin::utilityProvider();
}

} // namespace nx::vms_server_plugins::analytics::stub::http_requests
