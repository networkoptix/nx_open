// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "integration.h"

#include "engine.h"

namespace nx::vms_server_plugins::analytics::stub::http_requests {

using namespace nx::sdk;
using namespace nx::sdk::analytics;

Result<IEngine*> Integration::doObtainEngine()
{
    return new Engine(this);
}

std::string Integration::manifestString() const
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

nx::sdk::Ptr<nx::sdk::IUtilityProvider> Integration::utilityProvider() const
{
    return nx::sdk::analytics::Integration::utilityProvider();
}

} // namespace nx::vms_server_plugins::analytics::stub::http_requests
